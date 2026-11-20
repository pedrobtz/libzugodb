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

// ---------------------------------------------------------------------------
// function/numeric/test_type_resolution.test: a TINYINT or SMALLINT operand
// of a FLOAT or DOUBLE operation was read as the bytes of a double, so
// 1::TINYINT + 1::REAL was 1.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: TINYINT and SMALLINT operands of floating-point arithmetic") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT 1::TINYINT + 1::REAL, 1::TINYINT + 1::DOUBLE, 2::TINYINT * 2.5, "
                    "1::SMALLINT + 1.5") == Rows{{"2", "2", "5", "2.5"}});
    conn.Query("CREATE TABLE t(a TINYINT, b SMALLINT, d DOUBLE)");
    conn.Query("INSERT INTO t VALUES (1, 2, 0.5)");
    CHECK(Run(conn, "SELECT a + d, b * d, a / d, b - 1.5 FROM t") == Rows{{"1.5", "1", "2", "0.5"}});
}

// ---------------------------------------------------------------------------
// BLOB (#31; types/blob/test_blob*.test, function/string/md5.test,
// sha1.test, sha256.test, null_byte.test). Text to BLOB kept the escape
// text, so a BLOB held '\xAA' as four bytes, printed it as text and
// hashed and measured it as such; any cast to BLOB was accepted.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: text to BLOB reads \\xHH as one byte") {
    Database db;
    Connection conn(db);
    // test_blob_cast.test
    CHECK(Run(conn, "SELECT '\\x20\\x00\\xFF'::BLOB, '\\x20\\x00\\xFF'::BLOB::VARCHAR, "
                    "'\\x20\\x00\\xFF'::VARCHAR::BLOB::VARCHAR::BLOB") ==
          Rows{{" \\x00\\xFF", " \\x00\\xFF", " \\x00\\xFF"}});
    CHECK(conn.Query("SELECT '\\xAA'::BLOB").GetValue(0, 0).type().id() == LogicalTypeId::BLOB);
    // md5.test, sha1.test, sha256.test: of the two bytes.
    CHECK(Run(conn, "SELECT md5('\\xff\\xff'::BLOB), sha1('\\xff\\xff'::BLOB)") ==
          Rows{{"ab2a0d28de6b77ffdd6c72afead099ab", "a19f987b885f5a96069f4bc7f12b9e84ceba7dfa"}});
    CHECK(Run(conn, "SELECT sha256('\\xff\\xff'::BLOB)") ==
          Rows{{"ca2fd00fa001190744c15c317643ab092e7048ce086a243e2be9437c898de1bb"}});
    // null_byte.test
    CHECK(Run(conn, "SELECT chr(0)::blob") == Rows{{"\\x00"}});
    // test_blob.test, test_blob_function.test: a column, its length and ||.
    conn.Query("CREATE TABLE blobs (b BYTEA)");
    conn.Query("INSERT INTO blobs VALUES('\\xaa\\xff\\xaa'), ('\\xaa1199')");
    CHECK(Run(conn, "SELECT b, OCTET_LENGTH(b), b || '\\x5A\\x5A'::BYTEA FROM blobs") ==
          Rows{{"\\xAA\\xFF\\xAA", "3", "\\xAA\\xFF\\xAAZZ"}, {"\\xAA1199", "5", "\\xAA1199ZZ"}});
    CHECK(Run(conn, "SELECT count(*) FROM blobs WHERE b = '\\xAA1199'") == Rows{{"1"}});
    // test_blob_operator.test: MIN and MAX compare the bytes, unsigned.
    conn.Query("INSERT INTO blobs VALUES ('\\x00whatisgoingon'), ('\\xFF\\xFE\\xFB')");
    CHECK(Run(conn, "SELECT MIN(b), MAX(b) FROM blobs") ==
          Rows{{"\\x00whatisgoingon", "\\xFF\\xFE\\xFB"}});
}

TEST_CASE("Stage 8: text that is not a BLOB's is an error, NULL under TRY_CAST") {
    Database db;
    Connection conn(db);
    // test_blob_try_cast.test
    CHECK(Run(conn, "SELECT TRY_CAST('\\\\' AS BLOB), TRY_CAST('\\\\x3' AS BLOB), "
                    "TRY_CAST('\\\\xx2' AS BLOB), TRY_CAST('ü' AS BLOB)") ==
          Rows{{"NULL", "NULL", "NULL", "NULL"}});
    CHECK_THROWS(conn.Query("SELECT '\\\\'::BLOB"));
    // test_blob.test
    conn.Query("CREATE TABLE blobs (b BYTEA)");
    CHECK_THROWS(conn.Query("INSERT INTO blobs VALUES('\\xGA\\xFF\\xAA')"));
    CHECK_THROWS(conn.Query("INSERT INTO blobs VALUES('blablabla\\x')"));
    CHECK_THROWS(conn.Query("SELECT 'ü'::blob"));
    // test_blob_cast.test: DuckDB casts to a BLOB from text only.
    CHECK_THROWS(conn.Query("SELECT 1::BYTEA"));
    CHECK_THROWS(conn.Query("SELECT TRY_CAST(1 AS BLOB)"));
    CHECK_THROWS(conn.Query("INSERT INTO blobs VALUES (1)"));
    // The functions that would read a BLOB as text are refused.
    CHECK_THROWS(conn.Query("SELECT upper('a'::BLOB)"));
    CHECK_THROWS(conn.Query("SELECT 'a'::BLOB || 'b'::VARCHAR"));
}

// ---------------------------------------------------------------------------
// EXTRACT(WEEK) (#37; function/date/test_extract.test,
// test_extract_edge_cases.test): week 1 was counted from the Thursday of
// Jan 1's week, one week early when Jan 1 is a Friday, Saturday or Sunday.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: EXTRACT(WEEK) is the ISO week") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT EXTRACT(week FROM cast('2006-01-01' AS DATE)), "
                    "EXTRACT(week FROM DATE '2006-01-02'), EXTRACT(week FROM DATE '2008-12-29'), "
                    "EXTRACT(week FROM DATE '2010-01-03'), week(DATE '2004-12-31'), "
                    "date_part('week', TIMESTAMP '2021-01-03 10:00:00')") ==
          Rows{{"52", "1", "1", "53", "53", "53"}});
}
