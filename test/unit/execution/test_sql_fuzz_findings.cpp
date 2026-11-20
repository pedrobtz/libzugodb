// Defects the SQL fuzzer (fuzz/sql.cpp) and the checks around it found in
// Stage 9 (#13), and the function registry (design L26). Each expected
// value or error is DuckDB v2.0.0's for the same text.

#include "doctest.h"
#include "../conformance_rows.hpp"
#include "slothdb/main/database.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <string>

using namespace zugodb;
using namespace zugodb::conformance;

TEST_CASE("Stage 9: a condition is a BOOLEAN, another type cast as DuckDB's") {
    Database db;
    Connection conn(db);
    // `10 OR 1` read an INTEGER's byte as a bool (UBSan: load of value 10);
    // NOT 10 was true and CASE WHEN 2 took the ELSE.
    CHECK(Run(conn, "SELECT 10 OR 0, NOT 10, 0.5 OR false, CASE WHEN 2 THEN 1 ELSE 0 END") ==
          Rows{{"true", "false", "true", "1"}});
    CHECK(Run(conn, "SELECT 1 WHERE 5") == Rows{{"1"}});
    // DuckDB cannot read 'abc' as a BOOLEAN; the engine gave no row.
    CHECK_THROWS(conn.Query("SELECT 1 WHERE 'abc'"));
    CHECK_THROWS(conn.Query("SELECT 1 WHERE DATE '2020-01-01'"));
}

TEST_CASE("Stage 9: CASE, COALESCE, NULLIF, GREATEST and LEAST keep DuckDB's types") {
    Database db;
    Connection conn(db);
    auto r = conn.Query("SELECT CASE WHEN true THEN true ELSE false END, "
                        "coalesce(DATE '2020-01-01', NULL), nullif(true, false), "
                        "greatest(DATE '2020-01-01', DATE '2021-01-01'), "
                        "CASE WHEN true THEN 0.5::FLOAT END, CASE WHEN true THEN 1::TINYINT END");
    REQUIRE(r.RowCount() == 1);
    CHECK(r.column_types[0].ToString() == "BOOLEAN");
    CHECK(r.column_types[1].ToString() == "DATE");
    CHECK(r.column_types[2].ToString() == "BOOLEAN");
    CHECK(r.column_types[3].ToString() == "DATE");
    CHECK(r.column_types[4].ToString() == "FLOAT");
    CHECK(r.column_types[5].ToString() == "TINYINT");
    CHECK(Run(conn, "SELECT CASE WHEN false THEN 1 ELSE '2' END, coalesce(NULL, 1, 2.5)") ==
          Rows{{"2", "1"}});
    // A VARCHAR column beside a number: DuckDB's type depends on which is
    // a literal; refused.
    conn.Query("CREATE TABLE t(s VARCHAR, i INTEGER)");
    CHECK_THROWS(conn.Query("SELECT coalesce(s, i) FROM t"));
    CHECK_THROWS(conn.Query("SELECT CASE WHEN true THEN true ELSE 1 END"));
}

TEST_CASE("Stage 9: GENERATE_SERIES reads integer literals only, and counts any step") {
    Database db;
    Connection conn(db);
    // generate_series(-4, 4): the -4 was a UnaryMinusExpression cast to a
    // ConstantExpression (UBSan: downcast of address ... not an object of
    // that type), then std::stoll of garbage.
    CHECK(Run(conn, "SELECT count(*), min(generate_series), max(generate_series) "
                    "FROM generate_series(-4, 4)") == Rows{{"9", "-4", "4"}});
    // A negative step counted 0 rows and ran without the 10M limit.
    CHECK(Run(conn, "SELECT count(*) FROM generate_series(10, -10, -3)") == Rows{{"7"}});
    CHECK_THROWS(conn.Query("SELECT count(*) FROM generate_series(0, -100000000000, -1)"));
    CHECK(Run(conn, "SELECT count(*) FROM generate_series(9223372036854775806, "
                    "9223372036854775807)") == Rows{{"2"}});
    CHECK_THROWS(conn.Query("SELECT * FROM generate_series(1 + 1)"));
    CHECK_THROWS(conn.Query("SELECT * FROM generate_series(1, 2, 0)"));
}

TEST_CASE("Stage 9: function arguments of the types and number DuckDB's take") {
    Database db;
    Connection conn(db);
    // UPPER(x LIKE y) read a BOOLEAN as a string_t, past its buffer (ASan).
    CHECK_THROWS(conn.Query("SELECT upper('a' LIKE 'b')"));
    // date_part with one argument read past the argument list (ASan).
    CHECK_THROWS(conn.Query("SELECT date_part('quarter')"));
    conn.Query("CREATE TABLE t(s VARCHAR, b BOOLEAN, d DATE)");
    conn.Query("INSERT INTO t VALUES ('4', true, DATE '2020-01-01')");
    for (const char *sql : {"SELECT sqrt(s) FROM t", "SELECT abs(b) FROM t",
                            "SELECT round(d) FROM t", "SELECT sin(d) FROM t",
                            "SELECT length(b) FROM t", "SELECT lower(d) FROM t",
                            "SELECT extract(year FROM s) FROM t", "SELECT make_date(s, 1, 1) FROM t",
                            "SELECT chr(s) FROM t", "SELECT upper(1)", "SELECT abs()",
                            "SELECT pi(1)", "SELECT lpad('a')"}) {
        CAPTURE(sql);
        CHECK_THROWS(conn.Query(sql));
    }
    CHECK(Run(conn, "SELECT sqrt(4), upper(s), length(s) FROM t") == Rows{{"2", "4", "1"}});
}

TEST_CASE("Stage 9: names outside the registry are refused at bind time") {
    Database db;
    Connection conn(db);
    // Random numbers and UUIDs come from a host's provider (design L25).
    for (const char *sql : {"SELECT random()", "SELECT uuid()", "SELECT gen_random_uuid()",
                            "SELECT setseed(0.5)", "SELECT version()",
                            "SELECT current_database()", "SELECT no_such_function(1)",
                            "SELECT base64_encode('a')", "SELECT sha512('a')"}) {
        CAPTURE(sql);
        CHECK_THROWS(conn.Query(sql));
    }
    CHECK(Run(conn, "SELECT current_schema()") == Rows{{"main"}});
}

TEST_CASE("Stage 9: DESCRIBE, PRAGMA table_info and SHOW TABLES as DuckDB's") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE zb(i INTEGER PRIMARY KEY, j INTEGER NOT NULL, k INTEGER)");
    conn.Query("CREATE TABLE Ab(x INTEGER)");
    // DuckDB: null NO and key PRI; they were YES and NULL.
    CHECK(Run(conn, "DESCRIBE zb") ==
          Rows{{"i", "INTEGER", "NO", "PRI", "NULL", "NULL"},
               {"j", "INTEGER", "NO", "NULL", "NULL", "NULL"},
               {"k", "INTEGER", "YES", "NULL", "NULL", "NULL"}});
    CHECK(Run(conn, "PRAGMA table_info('zb')") ==
          Rows{{"0", "i", "INTEGER", "true", "NULL", "true"},
               {"1", "j", "INTEGER", "true", "NULL", "false"},
               {"2", "k", "INTEGER", "false", "NULL", "false"}});
    // In DuckDB's order, by name.
    conn.Query("CREATE TABLE b(x INTEGER)");
    conn.Query("CREATE TABLE \"C\"(x INTEGER)");
    CHECK(Run(conn, "SHOW TABLES") == Rows{{"Ab"}, {"C"}, {"b"}, {"zb"}});
    CHECK_THROWS(conn.Query("SHOW TABLES LIKE 'z%'"));
    CHECK_THROWS(conn.Query("SHOW COLUMNS FROM zb"));
}

// Every scalar function and aggregate in the registry reaches an
// implementation: called with its least number of NULL arguments, it gives
// a result or a binder's error, never the executor's "Function execution
// for" (a name listed but not computed) or a crash.
TEST_CASE("Stage 9: every function in the registry is computed") {
    std::ifstream in(std::string(ZUGODB_SOURCE_DIR) + "/src/binder/binder.cpp");
    REQUIRE(in.good());
    std::stringstream ss;
    ss << in.rdbuf();
    auto src = ss.str();
    auto begin = src.find("BEGIN FUNCTION REGISTRY");
    auto end = src.find("END FUNCTION REGISTRY");
    REQUIRE(begin != std::string::npos);
    REQUIRE(end != std::string::npos);
    auto block = src.substr(begin, end - begin);
    auto windows = block.find("kWindowFunctions");
    REQUIRE(windows != std::string::npos);
    auto aggregates = block.find("kAggregateFunctions");

    Database db;
    Connection conn(db);
    int seen = 0;
    std::regex scalar("\\{\"([A-Z0-9_]+)\", ([0-9]+), ");
    for (std::sregex_iterator it(block.begin(), block.begin() + aggregates, scalar), e; it != e;
         ++it) {
        std::string name = (*it)[1];
        int lo = std::stoi((*it)[2]);
        std::string sql = "SELECT " + name + "(";
        for (int k = 0; k < lo; k++) sql += k ? ", NULL" : "NULL";
        sql += ")";
        CAPTURE(sql);
        try {
            conn.Query(sql);
        } catch (const std::exception &ex) {
            CHECK(std::string(ex.what()).find("Function execution for") == std::string::npos);
        }
        seen++;
    }
    std::regex quoted("\"([A-Z0-9_]+)\"");
    for (std::sregex_iterator it(block.begin() + aggregates, block.begin() + windows, quoted), e;
         it != e; ++it) {
        std::string sql = "SELECT " + std::string((*it)[1]) + "(NULL)";
        CAPTURE(sql);
        try {
            conn.Query(sql);
        } catch (const std::exception &ex) {
            CHECK(std::string(ex.what()).find("Function execution for") == std::string::npos);
        }
        seen++;
    }
    CHECK(seen > 100);
}

TEST_CASE("Stage 9: a DOUBLE of 2^63 is out of BIGINT's range") {
    Database db;
    Connection conn(db);
    // 9223372036854775807 beside a DOUBLE in VALUES is 2^63 as a DOUBLE;
    // `d > (double)INT64_MAX` let it through to an undefined conversion
    // (UBSan: 9.22337e+18 is outside the range of 'long').
    CHECK_THROWS(conn.Query("SELECT v::BIGINT FROM (VALUES (0.5), (9223372036854775807)) x(v)"));
    CHECK_THROWS(conn.Query("SELECT 9223372036854775808.0::BIGINT"));
    CHECK(Run(conn, "SELECT (-9223372036854775808.0)::BIGINT, 9223372036854774784.0::BIGINT") ==
          Rows{{"-9223372036854775808", "9223372036854774784"}});
}

TEST_CASE("Stage 9: a decimal literal cast to an integer rounds half away from zero") {
    // DuckDB's decimal literal is a DECIMAL, whose cast rounds half away
    // from zero; the engine's is a DOUBLE, rounded half to even (2.5 gave
    // 2). Found beside the case above.
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT 0.5::BIGINT, 2.5::INTEGER, (-2.5)::INTEGER, 2.4::INTEGER, "
                    "-2.5::INTEGER, TRY_CAST(127.5 AS TINYINT), (-128.4)::TINYINT, "
                    "9223372036854775807.4::BIGINT, (-9223372036854775808.4)::BIGINT") ==
          Rows{{"1", "3", "-3", "2", "-3", "NULL", "-128", "9223372036854775807",
                "-9223372036854775808"}});
    CHECK_THROWS(conn.Query("SELECT 127.5::TINYINT"));
    // A DOUBLE rounds half to even, as DuckDB's DOUBLE does.
    CHECK(Run(conn, "SELECT 2.5::DOUBLE::INTEGER, 1e0::INTEGER, 25e-1::INTEGER") ==
          Rows{{"2", "1", "2"}});
}
