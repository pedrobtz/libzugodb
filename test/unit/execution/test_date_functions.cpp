#include "doctest.h"
#include "slothdb/main/database.hpp"
#include "slothdb/main/connection.hpp"

using namespace zugodb;

// Reference timestamp used throughout this file:
//   epoch seconds 1710499845 = 2024-03-15 10:50:45 UTC (a Friday).
//   Corresponding microsecond timestamp: 1710499845000000.

static constexpr int64_t TS_2024_03_15_10_50_45_US = 1710499845000000LL;

// ============================================================================
// DATE_TRUNC - extended interval parts
// ============================================================================

TEST_CASE("DATE_TRUNC - minute strips seconds") {
    Database db; Connection conn(db);
    auto r = conn.Query("SELECT DATE_TRUNC('minute', TIMESTAMP '2024-03-15 10:50:45')");
    // 2024-03-15 10:50:00 UTC = 1710499800 s
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 1710499800LL * 1000000LL);
}

TEST_CASE("DATE_TRUNC - second keeps seconds") {
    Database db; Connection conn(db);
    auto r = conn.Query("SELECT DATE_TRUNC('second', TIMESTAMP '2024-03-15 10:50:45')");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == TS_2024_03_15_10_50_45_US);
}

TEST_CASE("DATE_TRUNC - millisecond truncates sub-ms") {
    Database db; Connection conn(db);
    // Input already whole-second so ms truncation is a no-op but must round-trip correctly.
    auto r = conn.Query("SELECT DATE_TRUNC('millisecond', TIMESTAMP '2024-03-15 10:50:45')");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == TS_2024_03_15_10_50_45_US);
}

TEST_CASE("DATE_TRUNC - week snaps to Monday") {
    Database db; Connection conn(db);
    // 2024-03-15 is a Friday -> Monday of that week is 2024-03-11 00:00:00 UTC = 1710115200 s.
    auto r = conn.Query("SELECT DATE_TRUNC('week', TIMESTAMP '2024-03-15 10:50:45')");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 1710115200LL * 1000000LL);
}

TEST_CASE("DATE_TRUNC - week on a Monday is idempotent") {
    Database db; Connection conn(db);
    // 2024-03-11 00:00:00 UTC = 1710115200 s (this is already a Monday 00:00).
    auto r = conn.Query("SELECT DATE_TRUNC('week', TIMESTAMP '2024-03-11 00:00:00')");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 1710115200LL * 1000000LL);
}

TEST_CASE("DATE_TRUNC - week on a Sunday snaps back 6 days") {
    Database db; Connection conn(db);
    // 2024-03-17 is a Sunday. Week should snap back to Monday 2024-03-11.
    // 2024-03-17 10:00:00 UTC = 1710669600 s.
    auto r = conn.Query("SELECT DATE_TRUNC('week', TIMESTAMP '2024-03-17 10:00:00')");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 1710115200LL * 1000000LL);
}

TEST_CASE("DATE_TRUNC - quarter snaps to quarter start") {
    Database db; Connection conn(db);
    // March is in Q1 -> 2024-01-01 00:00:00 UTC = 1704067200 s.
    auto r = conn.Query("SELECT DATE_TRUNC('quarter', TIMESTAMP '2024-03-15 10:50:45')");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 1704067200LL * 1000000LL);
}

TEST_CASE("DATE_TRUNC - quarter for a Q3 timestamp") {
    Database db; Connection conn(db);
    // 2024-08-20 00:00:00 UTC = 1724112000 s. Q3 starts 2024-07-01 = 1719792000 s.
    auto r = conn.Query("SELECT DATE_TRUNC('quarter', TIMESTAMP '2024-08-20 00:00:00')");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 1719792000LL * 1000000LL);
}

TEST_CASE("DATE_TRUNC - decade snaps to decade start") {
    Database db; Connection conn(db);
    // 2024 -> 2020-01-01 00:00:00 UTC = 1577836800 s.
    auto r = conn.Query("SELECT DATE_TRUNC('decade', TIMESTAMP '2024-03-15 10:50:45')");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 1577836800LL * 1000000LL);
}

TEST_CASE("DATE_TRUNC - century snaps to century start") {
    Database db; Connection conn(db);
    // 2024 -> 2000-01-01 00:00:00 UTC = 946684800 s.
    auto r = conn.Query("SELECT DATE_TRUNC('century', TIMESTAMP '2024-03-15 10:50:45')");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 946684800LL * 1000000LL);
}

TEST_CASE("DATE_TRUNC - year still works (regression)") {
    Database db; Connection conn(db);
    // 2024-01-01 00:00:00 UTC = 1704067200 s.
    auto r = conn.Query("SELECT DATE_TRUNC('year', TIMESTAMP '2024-03-15 10:50:45')");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 1704067200LL * 1000000LL);
}

TEST_CASE("DATE_TRUNC - lowercase interval part accepted") {
    Database db; Connection conn(db);
    // StringUtil::Upper normalizes; same answer.
    auto r = conn.Query("SELECT DATE_TRUNC('quarter', TIMESTAMP '2024-03-15 10:50:45')");
    auto r2 = conn.Query("SELECT DATE_TRUNC('QUARTER', TIMESTAMP '2024-03-15 10:50:45')");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == r2.GetValue(0, 0).GetValue<int64_t>());
}

// ============================================================================
// MONTHNAME / DAYNAME
// ============================================================================

TEST_CASE("MONTHNAME - returns English month name") {
    Database db; Connection conn(db);
    auto r = conn.Query("SELECT MONTHNAME(TIMESTAMP '2024-03-15 10:50:45')");
    CHECK(r.GetValue(0, 0).GetValue<std::string>() == "March");
}

TEST_CASE("MONTHNAME - December boundary") {
    Database db; Connection conn(db);
    // 2024-12-31 12:00:00 UTC = 1735646400 s.
    auto r = conn.Query("SELECT MONTHNAME(TIMESTAMP '2024-12-31 12:00:00')");
    CHECK(r.GetValue(0, 0).GetValue<std::string>() == "December");
}

TEST_CASE("DAYNAME - returns English day-of-week") {
    Database db; Connection conn(db);
    // 2024-03-15 was a Friday.
    auto r = conn.Query("SELECT DAYNAME(TIMESTAMP '2024-03-15 10:50:45')");
    CHECK(r.GetValue(0, 0).GetValue<std::string>() == "Friday");
}

TEST_CASE("DAYNAME - Sunday boundary") {
    Database db; Connection conn(db);
    // 2024-03-17 12:00:00 UTC was a Sunday. = 1710676800 s.
    auto r = conn.Query("SELECT DAYNAME(TIMESTAMP '2024-03-17 12:00:00')");
    CHECK(r.GetValue(0, 0).GetValue<std::string>() == "Sunday");
}

// ============================================================================
// LAST_DAY
// ============================================================================

TEST_CASE("LAST_DAY - March 2024 -> March 31") {
    Database db; Connection conn(db);
    // 2024-03-31 00:00:00 UTC = 1711843200 s.
    auto r = conn.Query("SELECT LAST_DAY(TIMESTAMP '2024-03-15 10:50:45')");
    CHECK(r.GetValue(0, 0).GetValue<int32_t>() == 19813); // DATE: days since 1970-01-01
}

TEST_CASE("LAST_DAY - leap February 2024 -> Feb 29") {
    Database db; Connection conn(db);
    // 2024-02-10 12:00:00 UTC = 1707566400 s. Leap year -> last day Feb 29 = 2024-02-29.
    // 2024-02-29 00:00:00 UTC = 1709164800 s.
    auto r = conn.Query("SELECT LAST_DAY(TIMESTAMP '2024-02-10 12:00:00')");
    CHECK(r.GetValue(0, 0).GetValue<int32_t>() == 19782); // DATE: days since 1970-01-01
}

TEST_CASE("LAST_DAY - non-leap February 2023 -> Feb 28") {
    Database db; Connection conn(db);
    // 2023-02-10 12:00:00 UTC = 1676030400 s. Last day Feb 28 = 2023-02-28.
    // 2023-02-28 00:00:00 UTC = 1677542400 s.
    auto r = conn.Query("SELECT LAST_DAY(TIMESTAMP '2023-02-10 12:00:00')");
    CHECK(r.GetValue(0, 0).GetValue<int32_t>() == 19416); // DATE: days since 1970-01-01
}

// ============================================================================
// MAKE_DATE
// ============================================================================

TEST_CASE("MAKE_DATE - encodes YYYYMMDD") {
    Database db; Connection conn(db);
    auto r = conn.Query("SELECT MAKE_DATE(2024, 3, 15)");
    CHECK(r.GetValue(0, 0).GetValue<int32_t>() == 19797); // DATE: days since 1970-01-01
}

TEST_CASE("MAKE_DATE - single-digit month and day") {
    Database db; Connection conn(db);
    auto r = conn.Query("SELECT MAKE_DATE(2024, 1, 5)");
    CHECK(r.GetValue(0, 0).GetValue<int32_t>() == 19727); // DATE: days since 1970-01-01
}

TEST_CASE("MAKE_DATE - last day of year") {
    Database db; Connection conn(db);
    auto r = conn.Query("SELECT MAKE_DATE(2023, 12, 31)");
    CHECK(r.GetValue(0, 0).GetValue<int32_t>() == 19722); // DATE: days since 1970-01-01
}

// ============================================================================
// Stage 9 (#13, design L4): the MySQL forms SlothDB added follow DuckDB or
// are refused. Each expected value is DuckDB v2.0.0's for the same text.
// ============================================================================

#include "../conformance_rows.hpp"

using zugodb::conformance::Rows;
using zugodb::conformance::Run;

TEST_CASE("Stage 9: DATEDIFF and DATE_DIFF are DuckDB's (part, start, end)") {
    Database db; Connection conn(db);
    // DuckDB counts the part's boundaries; the engine counted whole units
    // elapsed (0, 0, 0 for the first three).
    CHECK(Run(conn, "SELECT datediff('month', DATE '2020-01-31', DATE '2020-02-01'), "
                    "date_diff('year', DATE '2020-12-31', DATE '2021-01-01'), "
                    "datediff('day', TIMESTAMP '2020-01-01 23:00:00', TIMESTAMP '2020-01-02 01:00:00'), "
                    "datediff('hour', TIMESTAMP '2020-01-01 10:00:00', TIMESTAMP '2020-01-02 09:00:00')") ==
          Rows{{"1", "1", "1", "23"}});
    // Before 1970 each operand is cut to its part rounding down.
    CHECK(Run(conn, "SELECT date_diff('second', TIMESTAMP '1969-12-31 23:59:59.5', "
                    "TIMESTAMP '1970-01-01 00:00:00.5'), date_diff('hour', "
                    "TIMESTAMP '1969-12-31 22:59:59', TIMESTAMP '1970-01-01 00:00:00'), "
                    "date_diff('millisecond', TIMESTAMP '1969-12-31 23:59:59.9995', "
                    "TIMESTAMP '1970-01-01 00:00:00')") == Rows{{"1", "2", "1"}});
    CHECK(Run(conn, "SELECT date_diff('quarter', DATE '2020-03-31', DATE '2020-04-01'), "
                    "date_diff('decade', DATE '2019-12-31', DATE '2020-01-01'), "
                    "date_diff('century', DATE '2000-12-31', DATE '2001-01-01'), "
                    "date_diff('millennium', DATE '2000-12-31', DATE '2001-01-01'), "
                    "date_diff('month', DATE '2020-03-01', DATE '2020-01-31')") ==
          Rows{{"1", "1", "0", "0", "-2"}});
    CHECK(Run(conn, "SELECT date_diff('day', DATE '2020-01-01', TIMESTAMP '2020-01-03 05:00:00'), "
                    "datediff('DAY', DATE '2020-01-01', DATE '2020-01-05')") == Rows{{"2", "4"}});
    // MySQL's DATEDIFF(d1, d2): DuckDB has no such function. It gave -4.
    CHECK_THROWS(conn.Query("SELECT datediff(DATE '2020-01-01', DATE '2020-01-05')"));
    // A part the engine does not count as DuckDB does, and a non-date.
    CHECK_THROWS(conn.Query("SELECT date_diff('week', DATE '2020-01-01', DATE '2020-01-15')"));
    CHECK_THROWS(conn.Query("SELECT date_diff('day', 1, 2)"));
}

TEST_CASE("Stage 9: DATE_ADD, DATE_SUB, SUBDATE, TIMESTAMPADD and the other MySQL forms are refused") {
    Database db; Connection conn(db);
    // DuckDB's date_sub('day', a, b) is 4 here; the engine read it as
    // MySQL's DATE_SUB(date, n, unit) and gave 1970-01-05.
    CHECK_THROWS(conn.Query("SELECT date_sub('day', DATE '2020-01-01', DATE '2020-01-05')"));
    // MySQL's DATE_ADD(date, n, unit): DuckDB raises. It gave 2020-01-04.
    CHECK_THROWS(conn.Query("SELECT date_add(DATE '2020-01-01', 3, 'day')"));
    for (const char *sql : {
             "SELECT subdate(DATE '2020-01-05', 2)", "SELECT adddate(DATE '2020-01-05', 2)",
             "SELECT timestampadd('day', 1, TIMESTAMP '2020-01-01')",
             "SELECT timestampdiff('day', DATE '2020-01-01', DATE '2020-01-05')",
             "SELECT dateadd('day', 1, DATE '2020-01-01')",
             "SELECT date_format(DATE '2020-01-01', '%Y')", "SELECT from_unixtime(0)",
             "SELECT unix_timestamp()", "SELECT curdate()", "SELECT getdate()",
             "SELECT nvl(NULL, 2)", "SELECT iif(true, 1, 2)"}) {
        CAPTURE(sql);
        CHECK_THROWS(conn.Query(sql));
    }
    // date + n is DuckDB's arithmetic and stays.
    CHECK(Run(conn, "SELECT DATE '2020-01-02' + 1, 1 + DATE '2020-01-02', DATE '2020-01-02' - 1") ==
          Rows{{"2020-01-03", "2020-01-03", "2020-01-01"}});
    // DuckDB has no TIMESTAMP + INTEGER (the engine added seconds), and its
    // TIMESTAMP - TIMESTAMP is an INTERVAL (the engine gave seconds).
    CHECK_THROWS(conn.Query("SELECT TIMESTAMP '2020-01-01' + 1"));
    CHECK_THROWS(conn.Query("SELECT TIMESTAMP '2020-01-02' - TIMESTAMP '2020-01-01'"));
    CHECK_THROWS(conn.Query("SELECT DATE '2020-01-02' + 1::BIGINT"));
}

TEST_CASE("Stage 9: DAYNAME and MONTHNAME of a DATE, and STRFTIME, as DuckDB's") {
    Database db; Connection conn(db);
    // A DATE was read as microseconds: every DATE was a Thursday in January.
    CHECK(Run(conn, "SELECT dayname(DATE '2020-01-06'), monthname(DATE '2020-03-06'), "
                    "dayname(DATE '1969-12-28'), monthname(DATE '0001-01-01')") ==
          Rows{{"Monday", "March", "Sunday", "January"}});
    // DuckDB's specifiers; the C library's %c, %x and %g differ, its %z is
    // the local zone, and %f was not one.
    CHECK(Run(conn, "SELECT strftime(TIMESTAMP '2020-01-05 13:04:05.123456', "
                    "'%c|%x|%X|%g|%f|%I %p|%j|%U %W %V %G|%z|%a %b')") ==
          Rows{{"2020-01-05 13:04:05|2020-01-05|13:04:05|123|123456|01 PM|005|01 00 01 2020|+00|Sun Jan"}});
    CHECK_THROWS(conn.Query("SELECT strftime(DATE '2020-01-05', '%e')"));
    CHECK_THROWS(conn.Query("SELECT strftime(DATE '2020-01-05', 'abc%')"));
    CHECK_THROWS(conn.Query("SELECT strftime(DATE '2020-01-05', '')"));
    CHECK_THROWS(conn.Query("SELECT strftime(1, '%Y')"));
}
