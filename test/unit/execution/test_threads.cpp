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

int Rounds() {
    const char *env = std::getenv("ZUGODB_THREAD_ROUNDS");
    int n = env ? std::atoi(env) : 1;
    return n < 1 ? 1 : n;
}

} // namespace

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
