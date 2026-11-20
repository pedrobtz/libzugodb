// Casts and types of the pinned DuckDB subset that gave a wrong answer or
// accepted what DuckDB refuses (libzugodb Stage 8, #12). Each test runs
// the smallest SQL of the case's shape and expects DuckDB's recorded
// result from the corpus file it names (test/conformance/corpus/test/
// sql/...), or an error where the engine refuses the statement.

#include "doctest.h"
#include "../conformance_rows.hpp"
#include "slothdb/main/database.hpp"

using namespace zugodb;
using namespace zugodb::conformance;

// ---------------------------------------------------------------------------
// TIME (#25; types/time/time_parsing.test, time_try_cast.test,
// test_time.test, test_time_2411.test). Every TIME the engine made from
// text was 00:00:00, and TIME '...' literals were refused.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: text to TIME, the cast, the literal and a column") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT CAST('12:34:56' AS TIME), TIME '12:34:56', '12:34:56.789'::TIME, "
                    "TIME '12:34:56.789'") ==
          Rows{{"12:34:56", "12:34:56", "12:34:56.789", "12:34:56.789"}});
    // time_parsing.test: the fraction past microseconds is cut, trailing
    // zeros are not printed.
    CHECK(Run(conn, "SELECT '14:42:04.35'::TIME::VARCHAR, '14:42:04.999999999'::TIME::VARCHAR, "
                    "'14:42:04.000000'::TIME::VARCHAR") ==
          Rows{{"14:42:04.35", "14:42:04.999999", "14:42:04"}});
    CHECK_THROWS(conn.Query("SELECT '50:42:04.500'::TIME"));
    // test_time.test: a column inserted from text reads back unchanged.
    conn.Query("CREATE TABLE times(i TIME)");
    conn.Query("INSERT INTO times VALUES ('00:01:20'), ('20:08:10.998'), ('20:08:10.33'), ('20:08:10.001')");
    CHECK(Run(conn, "SELECT i::VARCHAR FROM times") ==
          Rows{{"00:01:20"}, {"20:08:10.998"}, {"20:08:10.33"}, {"20:08:10.001"}});
    CHECK_THROWS(conn.Query("INSERT INTO times VALUES ('')"));
    // test_time_2411.test: a TIMESTAMP's time of day.
    CHECK(Run(conn, "SELECT TIMESTAMP '2008-01-01 00:01:20'::TIME") == Rows{{"00:01:20"}});
}

TEST_CASE("Stage 8: TRY_CAST to TIME") {
    Database db;
    Connection conn(db);
    // time_try_cast.test
    CHECK(Run(conn, "SELECT try_cast('' AS TIME), try_cast('    ' AS TIME), try_cast('11' AS TIME), "
                    "try_cast('11:11:A' AS TIME), try_cast('24:00:01' AS TIME), "
                    "try_cast('23:60:00' AS TIME), try_cast('23:59:60' AS TIME)") ==
          Rows{{"NULL", "NULL", "NULL", "NULL", "NULL", "NULL", "NULL"}});
    CHECK(Run(conn, "SELECT try_cast('11:' AS TIME), try_cast('11:11' AS TIME), "
                    "try_cast('11:11:' AS TIME), try_cast('  11:11:11  ' AS TIME), "
                    "try_cast('24:00:00' AS TIME), try_cast('11/11/11' AS TIME)") ==
          Rows{{"11:00:00", "11:11:00", "11:11:00", "11:11:11", "24:00:00", "00:00:00"}});
    // Text DuckDB may read but the engine does not: an error, not NULL.
    CHECK_THROWS(conn.Query("SELECT try_cast('11:11:11+02' AS TIME)"));
    CHECK_THROWS(conn.Query("SELECT try_cast('allballs' AS TIME)"));
}

// ---------------------------------------------------------------------------
// DATE and TIMESTAMP text (types/date/date_try_cast.test,
// test_incorrect_dates.test, date_parsing.test, types/timestamp/
// timestamp_try_cast.test, bc_timestamp.test). The parsers took only
// 'YYYY-MM-DD[ HH:MM:SS[.f]]', so TRY_CAST gave NULL for what DuckDB
// reads, and a day past the month's end rolled over (2001-02-29 was
// 2001-03-01).
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: text to DATE by DuckDB's grammar") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT try_cast('2001-02-29' AS DATE), try_cast('2004-02-29' AS DATE), "
                    "try_cast('2004/02/29' AS DATE), try_cast('2004/02-29' AS DATE), "
                    "try_cast('1111-11-111' AS DATE), try_cast('1111-11' AS DATE)") ==
          Rows{{"NULL", "2004-02-29", "2004-02-29", "NULL", "NULL", "NULL"}});
    CHECK_THROWS(conn.Query("SELECT '1993-02-29'::DATE"));
    conn.Query("CREATE TABLE dates(i DATE)");
    CHECK_THROWS(conn.Query("INSERT INTO dates VALUES ('1993-02-29')"));
    CHECK(Run(conn, "SELECT '  1992-9-20  '::DATE, 'epoch'::DATE") == Rows{{"1992-09-20", "1970-01-01"}});
    // Dates the engine cannot hold or whose functions it lacks: refused,
    // under TRY_CAST too (design L19).
    CHECK_THROWS(conn.Query("SELECT try_cast('infinity' AS DATE)"));
    CHECK_THROWS(conn.Query("SELECT try_cast('-infinity' AS DATE)"));
    CHECK_THROWS(conn.Query("SELECT try_cast('1111-11-11 (BC)' AS DATE)"));
    CHECK_THROWS(conn.Query("SELECT try_cast('10000-01-01' AS DATE)"));
    // Beyond DuckDB's own range: NULL.
    CHECK(Run(conn, "SELECT try_cast('5881580-07-11' AS DATE), try_cast('99999999-01-01' AS DATE)") ==
          Rows{{"NULL", "NULL"}});
}

TEST_CASE("Stage 8: text to TIMESTAMP by DuckDB's grammar, and its text") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT try_cast('1111-11-11 11:11' AS TIMESTAMP), "
                    "try_cast('2020-02-17T23:59:59.998Z' AS TIMESTAMP), "
                    "try_cast('2001-02-29 00:00:00' AS TIMESTAMP)") ==
          Rows{{"1111-11-11 11:11:00", "2020-02-17 23:59:59.998", "NULL"}});
    CHECK(Run(conn, "SELECT '1969-01-01 01:03:20.45432'::TIMESTAMP::VARCHAR") ==
          Rows{{"1969-01-01 01:03:20.45432"}});
    CHECK_THROWS(conn.Query("SELECT try_cast('infinity' AS TIMESTAMP)"));
    CHECK_THROWS(conn.Query("SELECT try_cast('2020-01-01 00:00:00+02' AS TIMESTAMP)"));
    CHECK_THROWS(conn.Query("SELECT 'epoch 00:00:00'::TIMESTAMP"));
    conn.Query("CREATE TABLE ts(t TIMESTAMP)");
    CHECK_THROWS(conn.Query("INSERT INTO ts VALUES ('1993-02-29 00:00:00')"));
}

// ---------------------------------------------------------------------------
// TIMESTAMP_S, TIMESTAMP_MS, TIMESTAMP_NS, TIMESTAMPTZ and TIMESTAMP(p)
// (types/timestamp/alternative_timestamp_casts.test, test_timestamp_tz.test,
// timestamp_precision.test) were the engine's TIMESTAMP under another
// name, so '...33.846'::TIMESTAMP_S kept its milliseconds where DuckDB
// rounds to 34, and TIMESTAMPTZ printed no offset: refused.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: the TIMESTAMP variants are refused") {
    Database db;
    Connection conn(db);
    const char *queries[] = {
        "SELECT '2024-05-10 11:06:33.846'::TIMESTAMP_S",
        "SELECT '2024-05-10 11:06:33.123846'::TIMESTAMP_MS",
        "SELECT '2023-12-08 08:51:39.123456'::TIMESTAMP_NS::TIME",
        "SELECT '2021-11-15 02:30:00'::TIMESTAMP::TIMESTAMPTZ",
        "SELECT '2020-01-01 01:23:45.123456789'::TIMESTAMP(3)",
        "CREATE TABLE t(a TIMESTAMP(0))",
        "CREATE TABLE t(a TIMETZ)",
    };
    for (auto *q : queries) {
        CAPTURE(q);
        CHECK_THROWS(conn.Query(q));
    }
    CHECK(Run(conn, "SELECT '2020-01-01 01:23:45.123456789'::TIMESTAMP(6), "
                    "'2020-01-01 01:23:45'::DATETIME") ==
          Rows{{"2020-01-01 01:23:45.123456", "2020-01-01 01:23:45"}});
}

// ---------------------------------------------------------------------------
// DECIMAL and NUMERIC (#26; types/decimal/*.test) bound to DOUBLE, so
// '0.1'::DECIMAL printed 0.1 for DuckDB's 0.100, sums lost digits and
// overflow went unchecked: refused until a fixed-point type exists.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: DECIMAL and NUMERIC are refused") {
    Database db;
    Connection conn(db);
    const char *queries[] = {
        "SELECT '0.1'::DECIMAL::VARCHAR",
        "SELECT CAST(0.1 AS DECIMAL(10,2)) + CAST(0.2 AS DECIMAL(10,2))",
        "SELECT TRY_CAST('9.99' AS DECIMAL(1,0))",
        "SELECT 1::NUMERIC(3,1)",
        "CREATE TABLE t(d DECIMAL(18,3))",
        "CREATE TABLE t(d NUMERIC)",
    };
    for (auto *q : queries) {
        CAPTURE(q);
        CHECK_THROWS(conn.Query(q));
    }
    // A decimal literal is a DOUBLE, as before.
    CHECK(Run(conn, "SELECT 0.5 + 1") == Rows{{"1.5"}});
}
