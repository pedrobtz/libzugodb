// Multi-threaded queries, for ThreadSanitizer (libzugodb Stage 5, #7).
//
// Each query runs at thread caps 1, 2 and 4 and must give the same rows at
// every cap: the single-threaded result is the oracle. The queries cover
// the engine's parallel paths that a query reaches:
//
// - over an in-memory table, the window operator sorts its partitions on
//   workers from 200,000 input rows and two partitions (PhysicalWindow::
//   Setup). GROUP BY, DISTINCT, ORDER BY, joins and plain aggregates over
//   an in-memory table run on the query's thread at every size; they are
//   here as the same oracle, and so that TSan sees them if that changes;
// - over a Parquet table (parquet_table.hpp), the scan's decode pipeline,
//   the row-group workers of aggregates, GROUP BY, DISTINCT and top-k, and
//   the ParallelFor merge of GROUP BY;
// - ParallelExecutor, which no query reaches, through its own API.
//
// A query marked `parallel` is also checked to reach a worker at cap 2,
// through the test seam, so that the loop cannot silently stop exercising
// threads. The CI job linux-tsan runs the whole suite under TSan, with
// ZUGODB_THREAD_ROUNDS set to repeat the loop at caps 2 and 4.

#include "doctest.h"
#include "../parquet_table.hpp"
#include "slothdb/common/exception.hpp"
#include "slothdb/common/parallel.hpp"
#include "slothdb/execution/parallel_executor.hpp"
#include "slothdb/main/connection.hpp"
#include "slothdb/main/database.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using namespace zugodb;

namespace {

// The window operator's threshold for sorting partitions on workers.
constexpr int64_t TABLE_ROWS = 200000;
constexpr int64_t TABLE_GROUPS = 37;
// test/fixtures/groups.parquet: k = i % 37, s = 's' || i % 11, v = i for
// i in 0 .. 16383, eight row groups (see test_worker_errors.cpp).
const std::string PARQUET_PATH = std::string(ZUGODB_SOURCE_DIR) + "/test/fixtures/groups.parquet";

struct ThreadCapGuard {
    unsigned int old;
    ThreadCapGuard() : old(ThreadCap()) {}
    ~ThreadCapGuard() { SetThreadCap(old); }
};

struct ArmedFault {
    explicit ArmedFault(WorkerSite site) { ArmWorkerFault(site); }
    ~ArmedFault() { DisarmWorkerFault(); }
};

// t(k, s, v, d, i): v = 0 .. TABLE_ROWS - 1, k = v % 37, s = 's' || v % 11,
// d = (v % 1000) / 4 (exact in binary, so sums do not depend on order),
// i = v % 5003. u: the same columns over 0 .. 999.
void MakeTable(Connection &conn, const std::string &name, int64_t rows) {
    conn.Query("CREATE TABLE " + name + " (k BIGINT, s VARCHAR, v BIGINT, d DOUBLE, i INTEGER)");
    for (int64_t base = 0; base < rows; base += (int64_t)VECTOR_SIZE) {
        int64_t n = std::min<int64_t>((int64_t)VECTOR_SIZE, rows - base);
        DataChunk chunk;
        chunk.Initialize({LogicalType::BIGINT(), LogicalType::VARCHAR(), LogicalType::BIGINT(),
                          LogicalType::DOUBLE(), LogicalType::INTEGER()});
        for (int64_t j = 0; j < n; j++) {
            int64_t v = base + j;
            chunk.SetValue(0, (idx_t)j, Value::BIGINT(v % TABLE_GROUPS));
            chunk.SetValue(1, (idx_t)j, Value::VARCHAR("s" + std::to_string(v % 11)));
            chunk.SetValue(2, (idx_t)j, Value::BIGINT(v));
            chunk.SetValue(3, (idx_t)j, Value::DOUBLE((double)(v % 1000) / 4.0));
            chunk.SetValue(4, (idx_t)j, Value::INTEGER((int32_t)(v % 5003)));
        }
        chunk.SetCardinality((idx_t)n);
        conn.Append(name, chunk);
    }
}

struct Case {
    const char *sql;
    bool ordered;  // the query fixes the row order, so compare it too
    bool parallel; // reaches a worker thread at cap 2
};

// Every ORDER BY here is a total order, and every window's ORDER BY is
// unique within its partition, so each result is deterministic.
const std::vector<Case> &Cases() {
    static const std::vector<Case> cases = {
        // Windows over the in-memory table: one per sort-key kind, and the
        // generic comparator of a two-key ORDER BY.
        {"SELECT k, v, row_number() OVER (PARTITION BY k ORDER BY v) FROM t", false, true},
        {"SELECT k, v, sum(v) OVER (PARTITION BY k ORDER BY v) FROM t", false, true},
        {"SELECT k, v, rank() OVER (PARTITION BY s ORDER BY d) FROM t", false, true},
        {"SELECT k, v, dense_rank() OVER (PARTITION BY k ORDER BY i DESC) FROM t", false, true},
        {"SELECT v, rank() OVER (PARTITION BY k ORDER BY s, d) FROM t", false, true},
        {"SELECT v, lag(v) OVER (PARTITION BY s ORDER BY v), lead(v) OVER (PARTITION BY s ORDER BY v) FROM t",
         false, true},
        {"SELECT k, count(*) OVER (PARTITION BY k), max(d) OVER (PARTITION BY k) FROM t", false, false},
        // The rest of the in-memory shapes: serial today.
        {"SELECT count(*), sum(v), min(v), max(v), avg(d), sum(d) FROM t", false, false},
        {"SELECT sum(v), count(*) FROM t WHERE k < 10 AND d > 3", false, false},
        {"SELECT k, sum(v), count(*), avg(d), min(i), max(i) FROM t GROUP BY k", false, false},
        {"SELECT s, count(*), sum(v) FROM t GROUP BY s", false, false},
        {"SELECT k, s, count(*) FROM t GROUP BY k, s", false, false},
        {"SELECT i, sum(v) FROM t GROUP BY i", false, false},
        {"SELECT v % 50000 AS g, count(*) FROM t GROUP BY g", false, false},
        {"SELECT count(DISTINCT k), count(DISTINCT s), count(DISTINCT i) FROM t", false, false},
        {"SELECT k, count(DISTINCT s) FROM t GROUP BY k", false, false},
        {"SELECT DISTINCT k, s FROM t", false, false},
        {"SELECT DISTINCT i FROM t", false, false},
        {"SELECT * FROM t ORDER BY v DESC", true, false},
        {"SELECT v, d FROM t ORDER BY d, v", true, false},
        {"SELECT * FROM t ORDER BY i, v LIMIT 100", true, false},
        {"SELECT s, v FROM t ORDER BY s DESC, v LIMIT 10", true, false},
        {"SELECT count(*), sum(t.v), sum(u.v) FROM t JOIN u ON t.k = u.k", false, false},
        {"SELECT t.k, count(*), sum(u.i) FROM t JOIN u ON t.k = u.k GROUP BY t.k", false, false},
        {"SELECT count(*), sum(b.v) FROM t a JOIN t b ON a.v = b.v", false, false},
        {"SELECT count(*), count(u.v) FROM t LEFT JOIN u ON t.v = u.v", false, false},
        {"SELECT k, sum(v) FROM t WHERE v IN (SELECT v FROM u) GROUP BY k", false, false},
        // The Parquet scan and the operators fed by it.
        {"SELECT * FROM pq", true, true},
        {"SELECT sum(v), count(*), min(v), max(v) FROM pq", false, true},
        {"SELECT k, sum(v) FROM pq GROUP BY k", false, true},
        {"SELECT s, count(*) FROM pq GROUP BY s", false, true},
        {"SELECT count(DISTINCT v) FROM pq", false, true},
        {"SELECT * FROM pq ORDER BY v DESC LIMIT 5", true, true},
        {"SELECT k, v FROM pq WHERE v % 7 = 3", false, false},
        {"SELECT k, row_number() OVER (PARTITION BY s ORDER BY v) FROM pq", false, false},
    };
    return cases;
}

// The result as one string per row, sorted unless the query fixes the order.
std::vector<std::string> Rows(Connection &conn, const Case &c) {
    auto r = conn.Query(c.sql);
    std::vector<std::string> out;
    out.reserve(r.RowCount());
    for (idx_t i = 0; i < r.RowCount(); i++) {
        std::string line;
        for (idx_t j = 0; j < r.ColumnCount(); j++) {
            if (j) line += '|';
            auto &v = r.GetValue(i, j);
            line += v.IsNull() ? std::string("NULL") : v.ToString();
        }
        out.push_back(std::move(line));
    }
    if (!c.ordered) std::sort(out.begin(), out.end());
    return out;
}

// The query fails through the seam armed at one of the sites a query can
// reach, that is, a worker body ran.
bool ReachesWorker(Connection &conn, const char *sql) {
    for (WorkerSite site : {WorkerSite::FORK_JOIN, WorkerSite::PARALLEL_FOR, WorkerSite::PIPELINE}) {
        ArmedFault fault(site);
        try {
            conn.Query(sql);
        } catch (const InternalException &) {
        }
        if (WorkerFaultHits() > 0) return true;
    }
    return false;
}

int Rounds() {
    const char *env = std::getenv("ZUGODB_THREAD_ROUNDS");
    int n = env ? std::atoi(env) : 1;
    return n < 1 ? 1 : n;
}

} // namespace

TEST_CASE("Threads - every query gives the same rows at thread caps 1, 2 and 4") {
    ThreadCapGuard cap;
    REQUIRE(std::filesystem::exists(PARQUET_PATH));
    Database db;
    Connection conn(db);
    MakeTable(conn, "t", TABLE_ROWS);
    MakeTable(conn, "u", 1000);
    zugodb::testing::AddParquetTable(db, "pq", PARQUET_PATH);

    SetThreadCap(1);
    std::vector<std::vector<std::string>> expected;
    for (auto &c : Cases()) expected.push_back(Rows(conn, c));

    const int rounds = Rounds();
    for (int round = 0; round < rounds; round++) {
        for (unsigned int n : {2u, 4u}) {
            SetThreadCap(n);
            for (size_t q = 0; q < Cases().size(); q++) {
                INFO("cap " << n << ", round " << round << ": " << Cases()[q].sql);
                auto got = Rows(conn, Cases()[q]);
                CHECK(got.size() == expected[q].size());
                CHECK(got == expected[q]);
            }
        }
    }
}

TEST_CASE("Threads - the parallel queries reach a worker thread at cap 2") {
    ThreadCapGuard cap;
    SetThreadCap(2);
    if (HWThreads() < 2) return; // a one-core host: no query has workers
    REQUIRE(std::filesystem::exists(PARQUET_PATH));
    Database db;
    Connection conn(db);
    MakeTable(conn, "t", TABLE_ROWS);
    MakeTable(conn, "u", 1000);
    zugodb::testing::AddParquetTable(db, "pq", PARQUET_PATH);
    for (auto &c : Cases()) {
        if (!c.parallel) continue;
        INFO(c.sql);
        CHECK(ReachesWorker(conn, c.sql));
    }
}

TEST_CASE("Threads - the window's row numbers match the table") {
    // An oracle independent of the engine for the parallel window sort: in
    // partition k = v % 37 ordered by v, row v is number v / 37 + 1. At
    // caps 2 and 4 the workers each set their partition's "sorted" flag,
    // which were bits of one std::vector<bool> before Stage 5 (a race TSan
    // reports).
    ThreadCapGuard cap;
    Database db;
    Connection conn(db);
    MakeTable(conn, "t", TABLE_ROWS);
    for (unsigned int n : {1u, 2u, 4u}) {
        SetThreadCap(n);
        INFO("cap " << n);
        auto r = conn.Query("SELECT v, row_number() OVER (PARTITION BY k ORDER BY v) FROM t");
        REQUIRE(r.RowCount() == (idx_t)TABLE_ROWS);
        idx_t bad = 0;
        for (idx_t i = 0; i < r.RowCount(); i++) {
            int64_t v = r.GetValue(i, 0).GetValue<int64_t>();
            int64_t rn = r.GetValue(i, 1).GetValue<int64_t>();
            if (rn != v / TABLE_GROUPS + 1) bad++;
        }
        CHECK(bad == 0);
    }
}

TEST_CASE("Threads - the Parquet scan's throttled decode workers give the file's rows") {
    // Eight row groups and at most 2 * workers + 2 decoded ahead, so at
    // caps 2 and 4 workers wait on the consumer's next_emit_, which the
    // consumer advanced outside the mutex before Stage 5 (a race TSan
    // reports). The oracle is the file's own definition.
    ThreadCapGuard cap;
    REQUIRE(std::filesystem::exists(PARQUET_PATH));
    Database db;
    Connection conn(db);
    zugodb::testing::AddParquetTable(db, "pq", PARQUET_PATH);
    constexpr int64_t PARQUET_ROWS = 16384;
    for (unsigned int n : {2u, 4u}) {
        SetThreadCap(n);
        INFO("cap " << n);
        for (int round = 0; round < Rounds(); round++) {
            auto r = conn.Query("SELECT k, v FROM pq WHERE v % 7 = 3");
            int64_t expect = 3;
            idx_t bad = 0;
            for (idx_t i = 0; i < r.RowCount(); i++, expect += 7) {
                int64_t k = r.GetValue(i, 0).GetValue<int64_t>();
                int64_t v = r.GetValue(i, 1).GetValue<int64_t>();
                if (v != expect || k != v % TABLE_GROUPS) bad++;
            }
            CHECK(r.RowCount() == (idx_t)((PARQUET_ROWS - 3 + 6) / 7));
            CHECK(bad == 0);
            auto all = conn.Query("SELECT v FROM pq");
            REQUIRE(all.RowCount() == (idx_t)PARQUET_ROWS);
            bad = 0;
            for (idx_t i = 0; i < all.RowCount(); i++)
                if (all.GetValue(i, 0).GetValue<int64_t>() != (int64_t)i) bad++;
            CHECK(bad == 0);
        }
    }
}

TEST_CASE("Threads - ParallelExecutor scans the same rows with 1, 2 and 4 threads") {
    // No query reaches ThreadPool; ParallelExecutor is its one user.
    ThreadCapGuard cap;
    Database db;
    Connection conn(db);
    MakeTable(conn, "t", TABLE_ROWS);
    auto &storage = db.GetCatalog().GetTable("t")->GetStorage();
    auto types = storage.GetTypes();
    auto rows = [&](unsigned int n) {
        SetThreadCap(n);
        auto got = ParallelExecutor::ParallelScanFilter(storage, nullptr, nullptr, types, n);
        std::vector<int64_t> vs;
        vs.reserve(got.size());
        for (auto &row : got) vs.push_back(row[2].GetValue<int64_t>());
        std::sort(vs.begin(), vs.end());
        return vs;
    };
    auto one = rows(1);
    REQUIRE(one.size() == (size_t)TABLE_ROWS);
    for (size_t i = 0; i < one.size(); i++) REQUIRE(one[i] == (int64_t)i);
    for (int round = 0; round < Rounds(); round++)
        for (unsigned int n : {2u, 4u}) CHECK(rows(n) == one);
}
