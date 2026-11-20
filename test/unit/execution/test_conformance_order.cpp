// ORDER BY, LIMIT and OFFSET in the pinned DuckDB subset that gave a wrong
// answer or accepted what DuckDB refuses (libzugodb Stage 8, #35). Each
// test runs the smallest SQL of the case's shape and expects DuckDB's
// recorded result from the corpus file it names (test/conformance/corpus/
// test/sql/...), or an error where the engine refuses the statement.

#include "doctest.h"
#include "../conformance_rows.hpp"
#include "zugodb/main/database.hpp"

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

// ---------------------------------------------------------------------------
// NULLs in an ORDER BY (#38). DuckDB's default_null_order is nulls_last,
// after ASC and DESC alike; the engine put them first after DESC. A
// window's ORDER BY compared a NULL's bytes as a value, ORDER BY x LIMIT
// k dropped the NULL rows, and with two keys it chose among the first
// key's ties as they came.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: NULLs sort last, in ORDER BY, under LIMIT and in a window") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE p(g INT, m INT)");
    conn.Query("INSERT INTO p VALUES (1, 3), (1, NULL), (1, 1), (1, 2)");
    CHECK(Run(conn, "SELECT m FROM p ORDER BY m DESC") == Rows{{"3"}, {"2"}, {"1"}, {"NULL"}});
    CHECK(Run(conn, "SELECT m FROM p ORDER BY m DESC NULLS FIRST") ==
          Rows{{"NULL"}, {"3"}, {"2"}, {"1"}});
    CHECK(Run(conn, "SELECT m FROM p ORDER BY m LIMIT 4") == Rows{{"1"}, {"2"}, {"3"}, {"NULL"}});
    CHECK(Run(conn, "SELECT m FROM p ORDER BY m NULLS FIRST LIMIT 2") == Rows{{"NULL"}, {"1"}});
    CHECK(Run(conn, "SELECT m, row_number() OVER (ORDER BY m), sum(m) OVER (ORDER BY m) "
                    "FROM p ORDER BY m") ==
          Rows{{"1", "1", "1"}, {"2", "2", "3"}, {"3", "3", "6"}, {"NULL", "4", "6"}});
    CHECK(Run(conn, "SELECT m FROM p QUALIFY row_number() OVER (PARTITION BY g ORDER BY m DESC) = 1") ==
          Rows{{"3"}});
    std::string values;
    for (int i = 1; i <= 100; i++) values += (i > 1 ? ", (1, " : "(1, ") + std::to_string(i) + ")";
    conn.Query("CREATE TABLE t(a INT, b INT)");
    conn.Query("INSERT INTO t VALUES " + values);
    CHECK(Run(conn, "SELECT b FROM t ORDER BY a, b DESC LIMIT 3") == Rows{{"100"}, {"99"}, {"98"}});
}
