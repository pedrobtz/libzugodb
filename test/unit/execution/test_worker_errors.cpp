// Errors on worker threads become query errors (libzugodb Stage 1, #3).
//
// An exception thrown on an engine worker thread used to end the process
// with std::terminate. Each kind of thread start in src/ (WorkerSite in
// slothdb/common/parallel.hpp) is made to throw here through the test seam,
// and the exception must arrive as an exception of Connection::Query (or of
// the API that started the workers), after which the same connection
// answers another query correctly.
//
// The Parquet cases reach the reader through read_parquet() from SQL. When
// Stage 2 refuses that, they move to the reader's C++ API or an internal
// path; the fork-join and pool cases need no file.

#include "doctest.h"
#include "slothdb/common/exception.hpp"
#include "slothdb/common/parallel.hpp"
#include "slothdb/execution/parallel_executor.hpp"
#include "slothdb/main/connection.hpp"
#include "slothdb/main/database.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace zugodb;

namespace {

// The window operator sorts partitions on workers from 200,000 rows.
constexpr int64_t TABLE_ROWS = 200000;
constexpr int64_t TABLE_GROUPS = 37;
// test/fixtures/groups.parquet, written by DuckDB with
// tools/make_parquet_fixtures.py: k = i % 37, s = 's' || i % 11, v = i for
// i in 0 .. 16383, eight row groups. (A file written by the engine's own
// ParquetWriter is not used: GROUP BY over it returns no rows.)
constexpr int64_t PARQUET_ROWS = 16384;
const std::string PARQUET_PATH = std::string(ZUGODB_SOURCE_DIR) + "/test/fixtures/groups.parquet";
const char *const SEAM_MESSAGE = "worker fault injected by the test seam";

// Every site needs at least two threads; the cap is the engine's own.
struct ThreadCapGuard {
    unsigned int old;
    ThreadCapGuard() : old(ThreadCap()) { SetThreadCap(2); }
    ~ThreadCapGuard() { SetThreadCap(old); }
};

struct ArmedFault {
    explicit ArmedFault(WorkerSite site) { ArmWorkerFault(site); }
    ~ArmedFault() { DisarmWorkerFault(); }
};

// t(k, s, v): v = 0 .. TABLE_ROWS - 1, k = v % 37, s = 's' || v % 11.
void MakeTable(Connection &conn) {
    conn.Query("CREATE TABLE t (k BIGINT, s VARCHAR, v BIGINT)");
    for (int64_t base = 0; base < TABLE_ROWS; base += (int64_t)VECTOR_SIZE) {
        int64_t n = std::min<int64_t>((int64_t)VECTOR_SIZE, TABLE_ROWS - base);
        DataChunk chunk;
        chunk.Initialize({LogicalType::BIGINT(), LogicalType::VARCHAR(), LogicalType::BIGINT()});
        for (int64_t i = 0; i < n; i++) {
            int64_t v = base + i;
            chunk.SetValue(0, (idx_t)i, Value::BIGINT(v % TABLE_GROUPS));
            chunk.SetValue(1, (idx_t)i, Value::VARCHAR("s" + std::to_string(v % 11)));
            chunk.SetValue(2, (idx_t)i, Value::BIGINT(v));
        }
        chunk.SetCardinality((idx_t)n);
        conn.Append("t", chunk);
    }
}

std::string ParquetSql(const std::string &select_list, const std::string &rest = "") {
    return "SELECT " + select_list + " FROM read_parquet('" + PARQUET_PATH + "')" + rest;
}

// The query fails with the seam's exception, raised on a worker of `site`.
void CheckQueryFails(Connection &conn, const std::string &sql, WorkerSite site) {
    std::string what;
    bool thrown = false;
    unsigned long long hits = 0;
    {
        ArmedFault fault(site);
        try {
            conn.Query(sql);
        } catch (const InternalException &e) {
            thrown = true;
            what = e.what();
        }
        hits = WorkerFaultHits();
    }
    INFO(sql);
    CHECK(hits > 0);
    REQUIRE(thrown);
    CHECK(what.find(SEAM_MESSAGE) != std::string::npos);
}

int64_t Int(const QueryResult &r, idx_t row, idx_t col) {
    return r.GetValue(row, col).GetValue<int64_t>();
}

// The connection still answers: count and sum over the in-memory table.
void CheckTableAnswers(Connection &conn) {
    auto r = conn.Query("SELECT count(*), sum(v) FROM t");
    REQUIRE(r.RowCount() == 1);
    CHECK(Int(r, 0, 0) == TABLE_ROWS);
    CHECK(Int(r, 0, 1) == TABLE_ROWS * (TABLE_ROWS - 1) / 2);
}

std::string ReadFile(const std::filesystem::path &p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

TEST_CASE("Worker errors - fork-join threads (WorkerGroup)") {
    ThreadCapGuard cap;
    REQUIRE(HWThreads() >= 2);
    Database db;
    Connection conn(db);
    MakeTable(conn);
    REQUIRE(std::filesystem::exists(PARQUET_PATH));

    SUBCASE("window partitions sorted on workers") {
        const std::string sql =
            "SELECT k, v, row_number() OVER (PARTITION BY k ORDER BY v) AS rn FROM t";
        CheckQueryFails(conn, sql, WorkerSite::FORK_JOIN);
        auto r = conn.Query(sql);
        CHECK(r.RowCount() == (idx_t)TABLE_ROWS);
        CheckTableAnswers(conn);
    }
    SUBCASE("Parquet row groups consumed on workers") {
        const std::string sql = ParquetSql("sum(v)");
        CheckQueryFails(conn, sql, WorkerSite::FORK_JOIN);
        auto r = conn.Query(sql);
        CHECK(Int(r, 0, 0) == PARQUET_ROWS * (PARQUET_ROWS - 1) / 2);
        CheckTableAnswers(conn);
    }
    SUBCASE("Parquet distinct count") {
        const std::string sql = ParquetSql("count(DISTINCT v)");
        CheckQueryFails(conn, sql, WorkerSite::FORK_JOIN);
        auto r = conn.Query(sql);
        CHECK(Int(r, 0, 0) == PARQUET_ROWS);
        CheckTableAnswers(conn);
    }
}

TEST_CASE("Worker errors - ParallelFor") {
    ThreadCapGuard cap;
    REQUIRE(HWThreads() >= 2);
    Database db;
    Connection conn(db);
    MakeTable(conn);
    REQUIRE(std::filesystem::exists(PARQUET_PATH));

    const std::string sql = ParquetSql("k, sum(v)", " GROUP BY k");
    CheckQueryFails(conn, sql, WorkerSite::PARALLEL_FOR);

    auto r = conn.Query(sql);
    REQUIRE(r.RowCount() == (idx_t)TABLE_GROUPS);
    std::map<int64_t, int64_t> expected;
    for (int64_t v = 0; v < PARQUET_ROWS; v++) expected[v % TABLE_GROUPS] += v;
    for (idx_t i = 0; i < r.RowCount(); i++) CHECK(Int(r, i, 1) == expected[Int(r, i, 0)]);
    CheckTableAnswers(conn);
}

TEST_CASE("Worker errors - Parquet decode pipeline") {
    ThreadCapGuard cap;
    REQUIRE(HWThreads() >= 2);
    Database db;
    Connection conn(db);
    MakeTable(conn);
    REQUIRE(std::filesystem::exists(PARQUET_PATH));

    const std::string sql = ParquetSql("*");
    CheckQueryFails(conn, sql, WorkerSite::PIPELINE);

    auto r = conn.Query(sql);
    REQUIRE(r.RowCount() == (idx_t)PARQUET_ROWS);
    CHECK(Int(r, PARQUET_ROWS - 1, 2) == PARQUET_ROWS - 1);
    CheckTableAnswers(conn);
}

TEST_CASE("Worker errors - thread pool tasks (ParallelExecutor)") {
    // No query reaches ThreadPool; ParallelExecutor is its one user, so the
    // error is checked at that API, then the connection answers.
    ThreadCapGuard cap;
    Database db;
    Connection conn(db);
    MakeTable(conn);
    auto &storage = db.GetCatalog().GetTable("t")->GetStorage();
    auto types = storage.GetTypes();

    bool thrown = false;
    unsigned long long hits = 0;
    {
        ArmedFault fault(WorkerSite::POOL);
        try {
            ParallelExecutor::ParallelScanFilter(storage, nullptr, nullptr, types, 2);
        } catch (const InternalException &e) {
            thrown = std::string(e.what()).find(SEAM_MESSAGE) != std::string::npos;
        }
        hits = WorkerFaultHits();
    }
    CHECK(hits > 0);
    CHECK(thrown);

    auto rows = ParallelExecutor::ParallelScanFilter(storage, nullptr, nullptr, types, 2);
    CHECK(rows.size() == (size_t)TABLE_ROWS);
    CheckTableAnswers(conn);
}

TEST_CASE("Worker errors - damaged Parquet file (SouravRoy-ETL/slothdb#33)") {
    // test/fixtures/damaged_zstd.parquet: written by DuckDB (x BIGINT, 0 to
    // 19999, ZSTD, ten row groups), then every zstd frame's magic number
    // damaged; tools/make_parquet_fixtures.py makes it. The page fails to
    // decompress on a decode worker. No seam is armed.
    ThreadCapGuard cap;
    REQUIRE(HWThreads() >= 2);
    const std::string path = std::string(ZUGODB_SOURCE_DIR) + "/test/fixtures/damaged_zstd.parquet";
    REQUIRE(std::filesystem::exists(path));
    Database db;
    Connection conn(db);
    MakeTable(conn);

    for (const char *select_list : {"sum(x)", "*"}) {
        const std::string sql =
            std::string("SELECT ") + select_list + " FROM read_parquet('" + path + "')";
        INFO(sql);
        bool thrown = false;
        try {
            conn.Query(sql);
        } catch (const IOException &e) {
            thrown = std::string(e.what()).find("decompression failed") != std::string::npos;
        }
        CHECK(thrown);
        CheckTableAnswers(conn);
    }
}

TEST_CASE("Worker errors - every thread start in src/ is a covered kind") {
    // A thread start that is not one of the kinds above fails here: start it
    // through WorkerGroup, ParallelFor or WorkerErrors, and add a doctest for
    // a new kind. The expected list is the reviewed inventory of Stage 1.
    namespace fs = std::filesystem;
    const fs::path root(ZUGODB_SOURCE_DIR);
    const std::regex start(R"(std::(j?thread)\b(?!::)|std::async\b|\.Submit\(|pthread_create|_beginthread|CreateThread\b)");
    const std::map<std::string, int> expected = {
        // WorkerGroup's thread vector, and three comments that name it.
        {"include/slothdb/common/parallel.hpp", 4},
        // The pool's own workers; its tasks carry exceptions in futures.
        {"include/slothdb/common/thread_pool.hpp", 1},
        // The pool's one user, under WorkerErrors (WorkerSite::POOL).
        {"src/execution/parallel_executor.cpp", 1},
        // PhysicalParquetScan's decode workers (PIPELINE, and FORK_JOIN in
        // RunParallelRGs), under its errors_.
        {"src/execution/physical_planner.cpp", 1},
    };
    std::map<std::string, int> found;
    for (const char *dir : {"src", "include"}) {
        for (auto &entry : fs::recursive_directory_iterator(root / dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext != ".cpp" && ext != ".hpp" && ext != ".h" && ext != ".c") continue;
            auto text = ReadFile(entry.path());
            auto n = std::distance(std::sregex_iterator(text.begin(), text.end(), start),
                                   std::sregex_iterator());
            if (n > 0) found[fs::relative(entry.path(), root).generic_string()] = (int)n;
        }
    }
    for (auto &[file, n] : found) {
        INFO(file);
        auto it = expected.find(file);
        CHECK(it != expected.end());
        if (it != expected.end()) CHECK(n == it->second);
    }
    CHECK(found.size() == expected.size());
}
