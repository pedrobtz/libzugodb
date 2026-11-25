// The cases of the pinned DuckDB subset that crashed or hung (libzugodb
// Stage 7, #11). Each test runs the case's SQL, or the smallest SQL of the
// same shape, and expects DuckDB's recorded result from the corpus file it
// names (test/conformance/corpus/test/sql/...), or an error where the
// engine refuses the statement.

#include "doctest.h"
#include "zugodb/main/connection.hpp"
#include "zugodb/main/database.hpp"

#include <string>
#include <vector>

using namespace zugodb;

namespace {

using Rows = std::vector<std::vector<std::string>>;

Rows Collect(const QueryResult &r) {
    Rows rows;
    for (idx_t i = 0; i < r.RowCount(); i++) {
        std::vector<std::string> row;
        for (idx_t j = 0; j < r.ColumnCount(); j++) {
            const auto &v = r.GetValue(i, j);
            row.push_back(v.IsNull() ? "NULL" : v.ToString());
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

Rows Run(Connection &conn, const std::string &sql) { return Collect(conn.Query(sql)); }

} // namespace

// ---------------------------------------------------------------------------
// String functions called with too few arguments
// (function/string/sha1.test, sha256.test, test_ascii.test,
// test_bit_length.test, test_repeat.test, test_replace.test,
// test_reverse.test, test_split_part.test, test_translate.test,
// test_trim.test, test_pad.test). DuckDB: "Binder Error: No function
// matches".
// ---------------------------------------------------------------------------

TEST_CASE("Stage 7: a string function with too few arguments is an error") {
    Database db;
    Connection conn(db);
    const char *queries[] = {
        "SELECT sha1()",        "SELECT sha256()",      "SELECT ASCII()",
        "SELECT BIT_LENGTH()",  "SELECT REPEAT()",      "SELECT REPEAT('a')",
        "SELECT REPLACE(1)",    "SELECT REPLACE(1, 2)", "SELECT REVERSE()",
        "SELECT split_part()",  "SELECT TRANSLATE(1)",  "SELECT TRANSLATE(1, 2)",
        "SELECT LTRIM()",       "SELECT RTRIM()",       "SELECT LPAD()",
        "SELECT LPAD(1)",       "SELECT LPAD(1, 2)",    "SELECT RPAD(1, 2)",
        "SELECT md5()",         "SELECT LENGTH()",      "SELECT UPPER()",
    };
    for (auto *q : queries) {
        CAPTURE(q);
        CHECK_THROWS(conn.Query(q));
    }
    // Too many arguments too (test_trim.test).
    CHECK_THROWS(conn.Query("SELECT LTRIM('hello', 'world', 'aaa')"));
    CHECK_THROWS(conn.Query("SELECT RTRIM('hello', 'world', 'aaa')"));
}

TEST_CASE("Stage 7: a string function with too few arguments is an error over a table") {
    // The arity is checked at bind time, so an empty table does not hide it.
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t(s VARCHAR)");
    CHECK_THROWS(conn.Query("SELECT REPLACE(s) FROM t"));
    conn.Query("INSERT INTO t VALUES ('a')");
    CHECK_THROWS(conn.Query("SELECT REPLACE(s) FROM t"));
}

// ---------------------------------------------------------------------------
// LPAD / RPAD (function/string/test_pad.test). LPAD('Base', -1, '-') hung.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 7: LPAD and RPAD with a negative or short or long length") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT LPAD('Base', 7, '-'), LPAD('Base', 4, '-'), "
                    "LPAD('Base', 2, ''), LPAD('Base', -1, '-')") ==
          Rows{{"---Base", "Base", "Ba", ""}});
    CHECK(Run(conn, "SELECT RPAD('Base', 7, '-'), RPAD('Base', 4, '-'), "
                    "RPAD('Base', 2, ''), RPAD('Base', -1, '-')") ==
          Rows{{"Base---", "Base", "Ba", ""}});
    CHECK(Run(conn, "SELECT LPAD('Base', 7, '-|'), LPAD('Base', 6, '-|'), "
                    "LPAD('Base', 5, '-|'), LPAD('Base', 4, '-|')") ==
          Rows{{"-|-Base", "-|Base", "-Base", "Base"}});
    CHECK(Run(conn, "SELECT LPAD('MotörHead', 16, 'RÄcks'), LPAD('MotörHead', 12, 'RÄcks'), "
                    "LPAD('MotörHead', 10, 'RÄcks')") ==
          Rows{{"RÄcksRÄMotörHead", "RÄcMotörHead", "RMotörHead"}});
    CHECK(Run(conn, "SELECT RPAD('MotörHead', 16, 'RÄcks'), RPAD('MotörHead', 12, 'RÄcks'), "
                    "RPAD('MotörHead', 10, 'RÄcks')") ==
          Rows{{"MotörHeadRÄcksRÄ", "MotörHeadRÄc", "MotörHeadR"}});
    CHECK(Run(conn, "SELECT LPAD('ab', 7, 'xy😀'), RPAD('ab', 7, 'xy😀')") ==
          Rows{{"xy😀xyab", "abxy😀xy"}});
    CHECK(Run(conn, "SELECT LPAD(NULL, 7, '-'), LPAD('Base', NULL, '-'), LPAD('Base', 7, NULL)") ==
          Rows{{"NULL", "NULL", "NULL"}});
    // No padding to fill with, and results past the maximum string size.
    CHECK_THROWS(conn.Query("SELECT LPAD('Hello', 10, '')"));
    CHECK_THROWS(conn.Query("SELECT RPAD('Hello', 10, '')"));
    CHECK_THROWS(conn.Query("SELECT LPAD('a', 100000000000000000, 'b')"));
    CHECK_THROWS(conn.Query("SELECT LPAD('', 1100000000, '😀')"));
    CHECK_THROWS(conn.Query("SELECT RPAD('😀😀', 1073741824, '😀')"));
    CHECK_THROWS(conn.Query("SELECT LPAD('abc', 2147483647, 'xyz😀😀')"));
}

// ---------------------------------------------------------------------------
// INSERT INTO t SELECT ... FROM t (#23; types/string/test_big_strings.test,
// types/blob/test_blob_string.test). The SELECT read the rows the INSERT
// appended, so the table grew without end.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 7: INSERT INTO t SELECT ... FROM t reads the table as it was") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t AS SELECT 1 AS x");
    conn.Query("INSERT INTO t SELECT x + 1 FROM t");
    CHECK(Run(conn, "SELECT x FROM t ORDER BY x") == Rows{{"1"}, {"2"}});

    conn.Query("CREATE TABLE u(x INTEGER)");
    conn.Query("INSERT INTO u VALUES (1), (2), (3)");
    conn.Query("INSERT INTO u SELECT x FROM u");
    CHECK(Run(conn, "SELECT count(*) FROM u") == Rows{{"6"}});
}

TEST_CASE("Stage 7: INSERT INTO t SELECT ... FROM t over a growing VARCHAR") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE test (a VARCHAR)");
    conn.Query("INSERT INTO test VALUES ('aaaaaaaaaa')");
    for (int i = 0; i < 3; i++) {
        conn.Query("INSERT INTO test SELECT a||a||a||a||a||a||a||a||a||a FROM test "
                   "WHERE LENGTH(a)=(SELECT MAX(LENGTH(a)) FROM test)");
    }
    CHECK(Run(conn, "SELECT LENGTH(a) FROM test ORDER BY 1") ==
          Rows{{"10"}, {"100"}, {"1000"}, {"10000"}});
}

// ---------------------------------------------------------------------------
// Window functions and ORDER BY (window/test_window_wisconsin.test,
// aggregate/qualify/test_qualify.test, aggregate/aggregates/
// test_skewness.test, window/test_streaming_window.test). The window
// operator emits the select list, but ORDER BY was bound to the source
// columns, so it read past the end of each row (a use after free or
// overflow in PhysicalOrderBy::Less) or sorted by the wrong column.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 7: ORDER BY a column the window query does not select") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t(a INTEGER, b INTEGER, c INTEGER)");
    conn.Query("INSERT INTO t VALUES (1, 30, 300), (2, 20, 100), (3, 10, 200)");
    CHECK(Run(conn, "SELECT row_number() OVER (ORDER BY a), b FROM t ORDER BY c") ==
          Rows{{"2", "20"}, {"3", "10"}, {"1", "30"}});
    CHECK(Run(conn, "SELECT b, row_number() OVER (ORDER BY c) AS r FROM t ORDER BY a") ==
          Rows{{"30", "3"}, {"20", "1"}, {"10", "2"}});
    // A selected column at another position than in the table.
    CHECK(Run(conn, "SELECT b, row_number() OVER (ORDER BY c) AS r FROM t ORDER BY b") ==
          Rows{{"10", "2"}, {"20", "1"}, {"30", "3"}});
    CHECK(Run(conn, "SELECT c, row_number() OVER (ORDER BY a) FROM t ORDER BY b") ==
          Rows{{"200", "3"}, {"100", "2"}, {"300", "1"}});
    // By the window's alias and by position.
    CHECK(Run(conn, "SELECT b, row_number() OVER (ORDER BY c) AS r FROM t ORDER BY r") ==
          Rows{{"20", "1"}, {"10", "2"}, {"30", "3"}});
    CHECK(Run(conn, "SELECT b, row_number() OVER (ORDER BY c) AS r FROM t ORDER BY 2 DESC") ==
          Rows{{"30", "3"}, {"10", "2"}, {"20", "1"}});
    // By an expression over source columns.
    CHECK(Run(conn, "SELECT b, row_number() OVER (ORDER BY c) AS r FROM t ORDER BY a + c") ==
          Rows{{"20", "1"}, {"10", "2"}, {"30", "3"}});
}

TEST_CASE("Stage 7: window/test_window_wisconsin.test shape") {
    // SELECT sum(four) OVER (PARTITION BY ten ORDER BY unique2) AS sum_1,
    // ten, four FROM tenk1 WHERE unique2 < 10 order by ten, unique2
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE tenk1(unique1 INTEGER, unique2 INTEGER, four INTEGER, ten INTEGER)");
    conn.Query("INSERT INTO tenk1 VALUES (10, 3, 2, 0), (20, 1, 1, 0), (30, 2, 3, 1), "
               "(40, 0, 1, 1), (50, 11, 1, 1)");
    CHECK(Run(conn, "SELECT sum(four) OVER (PARTITION BY ten ORDER BY unique2) AS sum_1, ten, four "
                    "FROM tenk1 WHERE unique2 < 10 ORDER BY ten, unique2") ==
          Rows{{"1", "0", "1"}, {"3", "0", "2"}, {"1", "1", "1"}, {"4", "1", "3"}});
}

TEST_CASE("Stage 7: aggregate/qualify/test_qualify.test shape") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t(unique1 INTEGER, four INTEGER, ten INTEGER)");
    conn.Query("INSERT INTO t VALUES (1, 1, 5), (2, 0, 3), (3, 1, 2), (4, 0, 4)");
    CHECK(Run(conn, "SELECT unique1 FROM t QUALIFY row_number() OVER (PARTITION BY four ORDER BY ten) = 1 "
                    "ORDER BY four, ten") == Rows{{"2"}, {"3"}});
}

TEST_CASE("Stage 7: window/test_streaming_window.test rank() over ()") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE integers(i INTEGER, j INTEGER)");
    conn.Query("INSERT INTO integers VALUES (2, 2), (2, 1), (1, 2), (1, NULL)");
    CHECK(Run(conn, "SELECT rank() OVER (), i, j FROM integers") ==
          Rows{{"1", "2", "2"}, {"1", "2", "1"}, {"1", "1", "2"}, {"1", "1", "NULL"}});
    CHECK(Run(conn, "SELECT dense_rank() OVER (), i, j FROM integers") ==
          Rows{{"1", "2", "2"}, {"1", "2", "1"}, {"1", "1", "2"}, {"1", "1", "NULL"}});
}

// ---------------------------------------------------------------------------
// aggregate/group/test_group_by_null_length.test
// ---------------------------------------------------------------------------

TEST_CASE("Stage 7: LENGTH(NULL) with GROUP BY NULL") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t0(c0 INTEGER)");
    conn.Query("INSERT INTO t0(c0) VALUES (1)");
    CHECK(Run(conn, "SELECT LENGTH(NULL) FROM t0 GROUP BY NULL") == Rows{{"NULL"}});
    CHECK(Run(conn, "SELECT c0, LENGTH(NULL) FROM t0 GROUP BY c0") == Rows{{"1", "NULL"}});
    CHECK(Run(conn, "SELECT NULL, LENGTH(NULL) FROM t0 GROUP BY NULL") == Rows{{"NULL", "NULL"}});
}

// ---------------------------------------------------------------------------
// aggregate/having/having_alias.test: an alias that is also a column.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 7: HAVING with an alias that shadows a grouped column") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t1(a INTEGER)");
    conn.Query("INSERT INTO t1 VALUES (42), (84)");
    // DuckDB: a reference that is both a group and an alias binds to the group.
    CHECK(Run(conn, "SELECT a+1 AS a FROM t1 GROUP BY a HAVING a=42") == Rows{{"43"}});
    conn.Query("CREATE TABLE t2(a INTEGER)");
    conn.Query("INSERT INTO t2 VALUES (42), (84), (42)");
    CHECK(Run(conn, "SELECT a AS b, sum(a) AS a FROM t2 GROUP BY b HAVING a=42") ==
          Rows{{"42", "84"}});
}

// ---------------------------------------------------------------------------
// function/date/test_date_trunc.test: the part from a column.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 7: date_trunc with the part from a column") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE dates(d DATE, s VARCHAR)");
    conn.Query("INSERT INTO dates VALUES ('1992-12-02', 'year'), ('1993-03-03', 'month'), "
               "('1994-05-05', 'day')");
    CHECK(Run(conn, "SELECT date_trunc(s, NULL::TIMESTAMP) FROM dates") ==
          Rows{{"NULL"}, {"NULL"}, {"NULL"}});
    CHECK(Run(conn, "SELECT date_trunc(NULL, d) FROM dates") ==
          Rows{{"NULL"}, {"NULL"}, {"NULL"}});
    // Each row as with its part written as a constant.
    auto per_row = Run(conn, "SELECT date_trunc(s, d::TIMESTAMP) FROM dates");
    auto constant = Run(conn, "SELECT date_trunc('year', DATE '1992-12-02'::TIMESTAMP), "
                              "date_trunc('month', DATE '1993-03-03'::TIMESTAMP), "
                              "date_trunc('day', DATE '1994-05-05'::TIMESTAMP)");
    REQUIRE(per_row.size() == 3);
    REQUIRE(constant.size() == 1);
    CHECK(per_row[0][0] == constant[0][0]);
    CHECK(per_row[1][0] == constant[0][1]);
    CHECK(per_row[2][0] == constant[0][2]);
    CHECK(constant[0][0] != constant[0][1]);
}

TEST_CASE("Stage 7: date_trunc with an unknown part is an error") {
    Database db;
    Connection conn(db);
    CHECK_THROWS(conn.Query("SELECT date_trunc('fortnight', TIMESTAMP '1992-02-02 02:02:03')"));
}

// ---------------------------------------------------------------------------
// function/string/test_substring.test: BIGINT bounds that may be NULL.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 7: substring with nullable BIGINT bounds under FILTER") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE nullable_substring_bounds(v VARCHAR NOT NULL, off BIGINT, len BIGINT)");
    conn.Query("INSERT INTO nullable_substring_bounds VALUES ('abc', NULL, 1), ('abc', 1, NULL), "
               "('abc', 1, 1)");
    CHECK(Run(conn, "SELECT count(*) FILTER (WHERE substring(v, off, 1) IS NULL), "
                    "count(*) FILTER (WHERE substring(v, 1, len) IS NULL), "
                    "count(*) FILTER (WHERE substring(v, off, len) IS NULL) "
                    "FROM nullable_substring_bounds") == Rows{{"1", "1", "2"}});
    CHECK(Run(conn, "SELECT substring(v, off, len) FROM nullable_substring_bounds") ==
          Rows{{"NULL"}, {"NULL"}, {"a"}});
    // Bounds past DuckDB's range, +-2^32: an error, as DuckDB's
    // (test_substring.test; Stage 8 made them errors).
    CHECK_THROWS(conn.Query("SELECT substring('hello', 2, 4294967297)"));
    CHECK_THROWS(conn.Query("SELECT substring('hello', 4294967298, 2)"));
    CHECK_THROWS(conn.Query("SELECT substring('hello', -4294967297, 2)"));
    CHECK(Run(conn, "SELECT substring('hello', -4294967296, 4294967295)") == Rows{{"hell"}});
}
