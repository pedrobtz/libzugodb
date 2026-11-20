// Aggregates of the pinned DuckDB subset that gave a wrong answer
// (libzugodb Stage 8, #12). Each test runs the smallest SQL of the case's
// shape and expects DuckDB's recorded result from the corpus file it names
// (test/conformance/corpus/test/sql/...), or an error where the engine
// refuses the statement.

#include "doctest.h"
#include "../conformance_rows.hpp"
#include "slothdb/main/database.hpp"

using namespace zugodb;
using namespace zugodb::conformance;

// ---------------------------------------------------------------------------
// aggregate/aggregates/test_aggregate_types.test, string_agg_union.test,
// test_aggregate_types_scalar.test: STRING_AGG ignored its separator, MIN
// and MAX of a BOOLEAN read every value as false, and an aggregate of a
// non-numeric expression (MIN('hello'), STRING_AGG(s || 'x', '-')) was
// NULL.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: STRING_AGG's separator, and MIN and MAX of a BOOLEAN") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE strings(g INTEGER, s VARCHAR)");
    conn.Query("INSERT INTO strings VALUES (1, 'hello'), (2, 'world'), (2, 'r'), (3, NULL)");
    CHECK(Run(conn, "SELECT STRING_AGG(s, ' '), STRING_AGG(s, ''), STRING_AGG(s) FROM strings") ==
          Rows{{"hello world r", "helloworldr", "hello,world,r"}});
    CHECK(Run(conn, "SELECT g, STRING_AGG(s, ', ') FROM strings GROUP BY g ORDER BY g") ==
          Rows{{"1", "hello"}, {"2", "world, r"}, {"3", "NULL"}});
    CHECK_THROWS(conn.Query("SELECT STRING_AGG(s, NULL) FROM strings"));
    CHECK_THROWS(conn.Query("SELECT STRING_AGG(s, s) FROM strings"));
    conn.Query("CREATE TABLE booleans(g INTEGER, b BOOLEAN)");
    conn.Query("INSERT INTO booleans VALUES (1, false), (1, true), (2, false), (3, NULL)");
    CHECK(Run(conn, "SELECT MIN(b), MAX(b) FROM booleans") == Rows{{"false", "true"}});
    CHECK(Run(conn, "SELECT g, MIN(b), MAX(b) FROM booleans GROUP BY g ORDER BY g") ==
          Rows{{"1", "false", "true"}, {"2", "false", "false"}, {"3", "NULL", "NULL"}});
}

TEST_CASE("Stage 8: an aggregate the engine does not compute is an error, not NULL") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t(i INTEGER, s VARCHAR)");
    conn.Query("INSERT INTO t VALUES (1, 'a'), (2, 'b')");
    const char *queries[] = {
        "SELECT MIN('hello')",
        "SELECT STRING_AGG('hello', ' ')",
        "SELECT MIN(s || 'x') FROM t",
        "SELECT STRING_AGG(s || 'x', '-') FROM t",
        // DuckDB's aggregates the engine lacks: over no rows they gave no
        // row (mode_rewrite.test, test_any_value_noninlined.test).
        "SELECT mode(i) FROM t WHERE false",
        "SELECT any_value(s) FROM t WHERE i = 0",
        "SELECT product(i) FROM t",
    };
    for (auto *q : queries) {
        CAPTURE(q);
        CHECK_THROWS(conn.Query(q));
    }
    CHECK(Run(conn, "SELECT MIN(i + 1), MAX(i * 2), SUM(i + 1) FROM t") == Rows{{"2", "4", "5"}});
}

// ---------------------------------------------------------------------------
// aggregate/aggregates/test_sum_fastpath.test: a SUM of BIGINTs is a
// double inside the hash aggregate, so past 2^53 it lost digits, and past
// BIGINT it wrapped. Refused there.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: a BIGINT SUM past 2^53 is refused, not rounded") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE b(g INTEGER, x BIGINT)");
    conn.Query("INSERT INTO b VALUES (1, 9007199254740993), (1, 1), (2, 5)");
    CHECK_THROWS(conn.Query("SELECT g, SUM(x) FROM b GROUP BY g"));
    CHECK_THROWS(conn.Query("SELECT sum(i) FROM generate_series(9007199254738944, 9007199254740991) t(i)"));
    CHECK(Run(conn, "SELECT SUM(x) FROM b WHERE g = 2") == Rows{{"5"}});
}

// ---------------------------------------------------------------------------
// aggregate/aggregates/test_simple_filter.test (#36): SUM of a FLOAT was
// typed FLOAT, and the aggregate's double sum written into that slot read
// back 0, with or without FILTER. DuckDB's SUM of a FLOAT is a DOUBLE.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: SUM of a FLOAT, with FILTER and over a window") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE issue3105(gender VARCHAR, pay FLOAT, s SMALLINT)");
    conn.Query("INSERT INTO issue3105 VALUES ('male', 100, 30000), ('male', 200, 30000), "
               "('male', 300, 1), ('female', 150, 1), ('female', 250, 1)");
    CHECK(Run(conn, "SELECT SUM(pay) FILTER (WHERE gender = 'male'), "
                    "SUM(pay) FILTER (WHERE gender = 'female'), SUM(pay) FROM issue3105") ==
          Rows{{"600", "400", "1000"}});
    CHECK(Run(conn, "SELECT SUM(pay) FROM issue3105 WHERE gender = 'female'") == Rows{{"400"}});
    // Over a window too, and a SMALLINT's sum is a BIGINT there.
    CHECK(Run(conn, "SELECT SUM(pay) OVER (), SUM(s) OVER () FROM issue3105 LIMIT 1") ==
          Rows{{"1000", "60003"}});
}
