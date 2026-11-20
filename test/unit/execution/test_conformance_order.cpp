// ORDER BY, LIMIT and OFFSET in the pinned DuckDB subset that gave a wrong
// answer or accepted what DuckDB refuses (libzugodb Stage 8, #35). Each
// test runs the smallest SQL of the case's shape and expects DuckDB's
// recorded result from the corpus file it names (test/conformance/corpus/
// test/sql/...), or an error where the engine refuses the statement.

#include "doctest.h"
#include "../conformance_rows.hpp"
#include "slothdb/main/database.hpp"

using namespace zugodb;
using namespace zugodb::conformance;

TEST_CASE("Stage 8: ORDER BY a non-integer literal is an error") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE test (a INTEGER, b INTEGER)");
    conn.Query("INSERT INTO test VALUES (11, 22), (12, 21), (13, 22)");
    // test_order_by.test, test_order_by_exceptions.test,
    // test_order_by_non_ordinal_literal.test
    CHECK_THROWS(conn.Query("SELECT a FROM test ORDER BY 'hello world', a"));
    CHECK_THROWS(conn.Query("SELECT a FROM test ORDER BY NULL, a"));
    CHECK_THROWS(conn.Query("SELECT a FROM test ORDER BY true, a"));
    CHECK_THROWS(conn.Query("SELECT a FROM test UNION SELECT b FROM test ORDER BY true"));
    // A cast is an expression, and an integer an ordinal.
    CHECK(Run(conn, "SELECT a FROM test ORDER BY CAST('a' AS VARCHAR), a") ==
          Rows{{"11"}, {"12"}, {"13"}});
    CHECK(Run(conn, "SELECT a, b FROM test ORDER BY 2, 1") ==
          Rows{{"12", "21"}, {"11", "22"}, {"13", "22"}});
}

TEST_CASE("Stage 8: ORDER BY over a UNION takes an output name or position only") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE test (a INTEGER)");
    conn.Query("INSERT INTO test VALUES (11), (12), (13)");
    // test_order_by.test: an expression over the result was skipped, and
    // the rows came in the order the UNION made them.
    CHECK_THROWS(conn.Query("SELECT a-10 AS k FROM test UNION SELECT a-10 AS l FROM test ORDER BY 1-k"));
    CHECK_THROWS(conn.Query("SELECT a-10 AS k FROM test UNION SELECT a-11 AS l FROM test ORDER BY a-11"));
    CHECK_THROWS(conn.Query("SELECT a FROM test UNION SELECT a FROM test ORDER BY 2"));
    CHECK(Run(conn, "SELECT a-10 AS k FROM test UNION SELECT a-10 AS l FROM test ORDER BY K DESC") ==
          Rows{{"3"}, {"2"}, {"1"}});
    CHECK(Run(conn, "SELECT a-10 AS k FROM test UNION SELECT a-10 AS l FROM test ORDER BY 1 LIMIT 2") ==
          Rows{{"1"}, {"2"}});
}

TEST_CASE("Stage 8: a negative LIMIT or OFFSET is an error") {
    Database db;
    Connection conn(db);
    // negative_offset.test
    CHECK_THROWS(conn.Query("SELECT * FROM generate_series(0,10,1) LIMIT 3 OFFSET -1"));
    CHECK_THROWS(conn.Query("SELECT * FROM generate_series(0,10,1) LIMIT -3"));
    CHECK(Run(conn, "SELECT * FROM generate_series(0,10,1) LIMIT 2 OFFSET 1") == Rows{{"1"}, {"2"}});
}

// ---------------------------------------------------------------------------
// aggregate/qualify/test_qualify.test (#38): a DATE, TIME or TIMESTAMP
// Value compared as neither less nor equal, so a sort through Values (a
// window's ORDER BY, ORDER BY, a UNION's dedup) left them as they came.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: DATE and TIMESTAMP values sort and deduplicate") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE p(plant VARCHAR, d DATE, t TIMESTAMP)");
    conn.Query("INSERT INTO p VALUES ('B', '2019-01-02', '2019-01-02 10:00:00'), "
               "('B', '2019-01-04', '2019-01-04 10:00:00'), ('B', '2019-01-03', '2019-01-03 10:00:00')");
    CHECK(Run(conn, "SELECT d FROM p ORDER BY d DESC") ==
          Rows{{"2019-01-04"}, {"2019-01-03"}, {"2019-01-02"}});
    CHECK(Run(conn, "SELECT d FROM p UNION SELECT d FROM p ORDER BY 1") ==
          Rows{{"2019-01-02"}, {"2019-01-03"}, {"2019-01-04"}});
    CHECK(Run(conn, "SELECT t FROM p ORDER BY t DESC LIMIT 1") == Rows{{"2019-01-04 10:00:00"}});
    CHECK(Run(conn, "SELECT d FROM p QUALIFY rank() OVER (PARTITION BY plant ORDER BY d DESC) = 2") ==
          Rows{{"2019-01-03"}});
}
