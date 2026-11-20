// Errors on worker threads become query errors (libzugodb Stage 1, #3).
//
// An exception thrown on an engine worker thread used to end the process
// with std::terminate. Each kind of thread start in src/ (WorkerSite in
// zugodb/common/parallel.hpp) is made to throw here through the test seam,
// and the exception must arrive as an exception of Connection::Query (or of
// the API that started the workers), after which the same connection
// answers another query correctly.
//
// No query reaches ParallelFor or the thread pool since the Parquet scan
// went (Stage 11, #16): those two are checked at their own API.

#include "doctest.h"
#include "zugodb/common/exception.hpp"
#include "zugodb/common/parallel.hpp"
#include "zugodb/execution/parallel_executor.hpp"
#include "zugodb/main/connection.hpp"
#include "zugodb/main/database.hpp"

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

    SUBCASE("window partitions sorted on workers") {
        const std::string sql =
            "SELECT k, v, row_number() OVER (PARTITION BY k ORDER BY v) AS rn FROM t";
        CheckQueryFails(conn, sql, WorkerSite::FORK_JOIN);
        auto r = conn.Query(sql);
        CHECK(r.RowCount() == (idx_t)TABLE_ROWS);
        CheckTableAnswers(conn);
    }
}

TEST_CASE("Worker errors - ParallelFor") {
    // No query reaches ParallelFor; it is checked at its API, and runs
    // again once the seam is disarmed.
    ThreadCapGuard cap;
    REQUIRE(HWThreads() >= 2);
    bool thrown = false;
    unsigned long long hits = 0;
    {
        ArmedFault fault(WorkerSite::PARALLEL_FOR);
        try {
            ParallelFor(2, [](unsigned int) {});
        } catch (const InternalException &e) {
            thrown = std::string(e.what()).find(SEAM_MESSAGE) != std::string::npos;
        }
        hits = WorkerFaultHits();
    }
    CHECK(hits > 0);
    CHECK(thrown);

    std::vector<int> ran(2, 0);
    ParallelFor(2, [&](unsigned int t) { ran[t] = 1; });
    CHECK(ran == std::vector<int>{1, 1});
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

TEST_CASE("Worker errors - every thread start in src/ is a covered kind") {
    // A thread start that is not one of the kinds above fails here: start it
    // through WorkerGroup, ParallelFor or WorkerErrors, and add a doctest for
    // a new kind. The expected list is the reviewed inventory of Stage 1.
    namespace fs = std::filesystem;
    const fs::path root(ZUGODB_SOURCE_DIR);
    const std::regex start(R"(std::(j?thread)\b(?!::)|std::async\b|\.Submit\(|pthread_create|_beginthread|CreateThread\b)");
    const std::map<std::string, int> expected = {
        // WorkerGroup's thread vector, and three comments that name it.
        {"include/zugodb/common/parallel.hpp", 4},
        // The pool's own workers; its tasks carry exceptions in futures.
        {"include/zugodb/common/thread_pool.hpp", 1},
        // The pool's one user, under WorkerErrors (WorkerSite::POOL).
        {"src/execution/parallel_executor.cpp", 1},
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
