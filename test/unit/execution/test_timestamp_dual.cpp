#include "doctest.h"
#include "slothdb/main/database.hpp"
#include "slothdb/main/connection.hpp"

using namespace zugodb;

// ============================================================================
// DUAL / SELECT without FROM
// ============================================================================

TEST_CASE("DUAL - SELECT constant without FROM") {
    Database db;
    Connection conn(db);

    auto r = conn.Query("SELECT 42");
    CHECK(r.RowCount() == 1);
    CHECK(r.GetValue(0, 0).GetValue<int32_t>() == 42);
}

TEST_CASE("DUAL - SELECT string constant") {
    Database db;
    Connection conn(db);

    auto r = conn.Query("SELECT 'hello world'");
    CHECK(r.RowCount() == 1);
    CHECK(r.GetValue(0, 0).GetValue<std::string>() == "hello world");
}

TEST_CASE("DUAL - SELECT expression") {
    Database db;
    Connection conn(db);

    auto r = conn.Query("SELECT 2 + 3");
    CHECK(r.RowCount() == 1);
    CHECK(r.GetValue(0, 0).GetValue<int32_t>() == 5);
}

// ============================================================================
// Timestamp functions
// ============================================================================

TEST_CASE("Timestamp - NOW() returns value") {
    // DuckDB's NOW() is a TIMESTAMP WITH TIME ZONE in the session's time
    // zone; the engine has neither, so it is refused (Stage 9, design L26).
    Database db;
    Connection conn(db);
    CHECK_THROWS(conn.Query("SELECT NOW()"));
}

TEST_CASE("Timestamp - CURRENT_TIMESTAMP") {
    Database db;
    Connection conn(db);
    CHECK_THROWS(conn.Query("SELECT CURRENT_TIMESTAMP"));
}

TEST_CASE("Timestamp - CURRENT_DATE") {
    // DuckDB's is the date in the session's time zone; the engine's was
    // the UTC date (Stage 9, design L26).
    Database db;
    Connection conn(db);
    CHECK_THROWS(conn.Query("SELECT CURRENT_DATE"));
}

TEST_CASE("Timestamp - EXTRACT YEAR/MONTH/DAY") {
    Database db;
    Connection conn(db);

    conn.Query("CREATE TABLE ts (t TIMESTAMP)");
    conn.Query("INSERT INTO ts VALUES (TIMESTAMP '2024-06-15 12:30:00')");

    auto r = conn.Query("SELECT EXTRACT(YEAR FROM t) FROM ts");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 2024);

    r = conn.Query("SELECT EXTRACT(MONTH FROM t) FROM ts");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 6);

    r = conn.Query("SELECT EXTRACT(DAY FROM t) FROM ts");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 15);

    r = conn.Query("SELECT EXTRACT(HOUR FROM t) FROM ts");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 12);

    // DuckDB has no EXTRACT from a BIGINT; the engine read one as
    // seconds or microseconds by its size (Stage 9).
    conn.Query("CREATE TABLE b (t BIGINT)");
    conn.Query("INSERT INTO b VALUES (1718451000000000)");
    CHECK_THROWS(conn.Query("SELECT EXTRACT(YEAR FROM t) FROM b"));
}

TEST_CASE("Timestamp - TO_TIMESTAMP") {
    // DuckDB's TO_TIMESTAMP is a TIMESTAMP WITH TIME ZONE: refused.
    Database db;
    Connection conn(db);
    CHECK_THROWS(conn.Query("SELECT TO_TIMESTAMP(1718451000)"));
}

TEST_CASE("Timestamp - NOW() with table") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t (x INTEGER)");
    conn.Query("INSERT INTO t VALUES (1)");
    CHECK_THROWS(conn.Query("SELECT x, NOW() FROM t"));
}
