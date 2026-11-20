// Window functions of the pinned DuckDB subset that gave a wrong answer
// (libzugodb Stage 8, #12; design L21). Each test runs the smallest SQL of
// the case's shape and expects DuckDB's recorded result from the corpus
// file it names (test/conformance/corpus/test/sql/...), or an error where
// the engine refuses the statement.

#include "doctest.h"
#include "../conformance_rows.hpp"
#include "slothdb/main/database.hpp"

using namespace zugodb;
using namespace zugodb::conformance;

namespace {

void MakeT(Connection &conn) {
    conn.Query("CREATE TABLE t(p INTEGER, a INTEGER, b INTEGER, v INTEGER)");
    conn.Query("INSERT INTO t VALUES (1,1,1,10),(1,1,2,20),(1,2,1,30),(1,2,1,40),(2,1,1,50)");
}

} // namespace

// ---------------------------------------------------------------------------
// Shapes the window operator computed wrongly (aggregate/aggregates/
// test_regression.test, test_corr.test, test_kurtosis.test, ...: NULL;
// window/test_evil_window.test, test_scalar_window.test: an expression
// argument read as 0; window/test_basic_window.test,
// test_duplicate_partition.test: peers by the first ORDER BY key alone;
// test_no_default_window_spec.test, aggregate/qualify/test_qualify.test:
// LAST_VALUE over the whole partition; a running frame beside a window
// over another order: the whole partition). Refused.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: window shapes the operator does not compute are refused") {
    Database db;
    Connection conn(db);
    MakeT(conn);
    const char *queries[] = {
        "SELECT regr_slope(v, a) OVER (PARTITION BY p) FROM t",
        "SELECT stddev_pop(v) OVER (PARTITION BY p) FROM t",
        "SELECT percent_rank() OVER () FROM t",
        "SELECT sum(v * 2) OVER (PARTITION BY p ORDER BY a) FROM t",
        "SELECT avg(42) OVER () FROM t",
        "SELECT count(DISTINCT p) OVER () FROM t",
        "SELECT sum(v) FILTER (WHERE v > 20) OVER () FROM t",
        "SELECT lag(v, 1, 0) OVER (ORDER BY v) FROM t",
        "SELECT rank() OVER (PARTITION BY p ORDER BY a, b) FROM t",
        "SELECT sum(v) OVER (PARTITION BY p ORDER BY a, b) FROM t",
        "SELECT rank() OVER (ORDER BY a + b) FROM t",
        "SELECT last_value(v) OVER (PARTITION BY p ORDER BY a) FROM t",
        "SELECT sum(v) OVER (PARTITION BY p ORDER BY a), row_number() OVER (ORDER BY v) FROM t",
        "SELECT v FROM t QUALIFY row_number() OVER (ORDER BY v) <= 2 AND v > 10",
    };
    for (auto *q : queries) {
        CAPTURE(q);
        CHECK_THROWS(conn.Query(q));
    }
}

TEST_CASE("Stage 8: the window shapes the operator computes") {
    Database db;
    Connection conn(db);
    MakeT(conn);
    CHECK(Run(conn, "SELECT v, sum(v) OVER (PARTITION BY p ORDER BY a) FROM t ORDER BY v") ==
          Rows{{"10", "30"}, {"20", "30"}, {"30", "100"}, {"40", "100"}, {"50", "50"}});
    CHECK(Run(conn, "SELECT v, rank() OVER (PARTITION BY p ORDER BY a) FROM t ORDER BY v") ==
          Rows{{"10", "1"}, {"20", "1"}, {"30", "3"}, {"40", "3"}, {"50", "1"}});
    CHECK(Run(conn, "SELECT v, row_number() OVER (PARTITION BY p ORDER BY a, b, v) FROM t ORDER BY v") ==
          Rows{{"10", "1"}, {"20", "2"}, {"30", "3"}, {"40", "4"}, {"50", "1"}});
    CHECK(Run(conn, "SELECT v, lag(v) OVER (ORDER BY a, b, v) FROM t ORDER BY v") ==
          Rows{{"10", "NULL"}, {"20", "50"}, {"30", "20"}, {"40", "30"}, {"50", "10"}});
}
