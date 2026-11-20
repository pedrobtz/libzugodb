// The JSON reader and writer through their C++ API. SQL cannot read or
// write a file since Stage 2 (#4); the cases that did so check that it is
// refused, over files that exist.

#include "doctest.h"
#include "../refused_files.hpp"
#include "slothdb/main/database.hpp"
#include "slothdb/main/connection.hpp"
#include "slothdb/storage/json_reader.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace zugodb;
using zugodb::testing::CheckRefused;

static void cleanup_files() {
    std::remove("test.json");
    std::remove("test.ndjson");
    std::remove("test_out.json");
    std::remove("test_data2.csv");
}

// ============================================================================
// JSON Reader/Writer
// ============================================================================

TEST_CASE("JSON - read JSON array") {
    {
        std::ofstream f("test.json");
        f << R"([
  {"id": 1, "name": "Alice", "score": 95.5},
  {"id": 2, "name": "Bob", "score": 87.3},
  {"id": 3, "name": "Charlie", "score": 92.1}
])";
    }

    JSONReader reader("test.json");
    reader.DetectSchema();
    auto names = reader.GetColumnNames();
    auto types = reader.GetColumnTypes();
    auto rows = reader.ReadAll();

    CHECK(names.size() == 3);
    CHECK(rows.size() == 3);
    CHECK(rows[0][0].GetValue<int32_t>() == 1);
    CHECK(rows[0][1].GetValue<std::string>() == "Alice");

    cleanup_files();
}

TEST_CASE("JSON - read NDJSON") {
    {
        std::ofstream f("test.ndjson");
        f << R"({"x": 10, "y": "hello"})" << "\n";
        f << R"({"x": 20, "y": "world"})" << "\n";
    }

    JSONOptions opts;
    opts.array_format = false;
    JSONReader reader("test.ndjson", opts);
    reader.DetectSchema();
    auto rows = reader.ReadAll();
    CHECK(rows.size() == 2);
    CHECK(rows[0][0].GetValue<int32_t>() == 10);
    CHECK(rows[1][1].GetValue<std::string>() == "world");

    cleanup_files();
}

TEST_CASE("JSON Writer - write and read back") {
    {
        JSONWriter writer("test_out.json");
        writer.WriteHeader({"id", "name"});
        writer.WriteRow({"id", "name"}, {Value::INTEGER(1), Value::VARCHAR("Alice")});
        writer.WriteRow({"id", "name"}, {Value::INTEGER(2), Value::VARCHAR("Bob")});
        writer.Finish();
    }

    JSONReader reader("test_out.json");
    reader.DetectSchema();
    auto rows = reader.ReadAll();
    CHECK(rows.size() == 2);

    cleanup_files();
}

// ============================================================================
// read_json() table function: refused
// ============================================================================

TEST_CASE("read_json - refused over a JSON file") {
    {
        std::ofstream f("test.json");
        f << R"([{"id": 1, "name": "Alice"}, {"id": 2, "name": "Bob"}])";
    }

    Database db;
    Connection conn(db);
    CheckRefused(conn, "SELECT * FROM read_json('test.json')");
    CheckRefused(conn, "SELECT * FROM read_json_auto('test.json')");

    cleanup_files();
}

TEST_CASE("read_json - refused with a WHERE filter, and read_ndjson") {
    {
        std::ofstream f("test.ndjson");
        f << R"({"x": 1})" << "\n" << R"({"x": 4})" << "\n" << R"({"x": 5})" << "\n";
    }

    Database db;
    Connection conn(db);
    CheckRefused(conn, "SELECT x FROM read_json('test.ndjson') WHERE x > 3");
    CheckRefused(conn, "SELECT x FROM read_ndjson('test.ndjson') WHERE x > 3");

    cleanup_files();
}

// ============================================================================
// read_csv() table function: refused
// ============================================================================

TEST_CASE("read_csv - refused over a CSV file") {
    {
        std::ofstream f("test_data2.csv");
        f << "a,b\n1,hello\n2,world\n";
    }

    Database db;
    Connection conn(db);
    CheckRefused(conn, "SELECT * FROM read_csv('test_data2.csv')");
    CheckRefused(conn, "SELECT * FROM read_csv_auto('test_data2.csv')");

    cleanup_files();
}

// ============================================================================
// A path used as a table: refused
// ============================================================================

TEST_CASE("Auto-detect - a CSV path is refused") {
    {
        std::ofstream f("test_data2.csv");
        f << "x,y\n10,hello\n20,world\n";
    }

    Database db;
    Connection conn(db);
    CheckRefused(conn, "SELECT * FROM 'test_data2.csv'");
    CheckRefused(conn, "DESCRIBE 'test_data2.csv'");

    cleanup_files();
}

TEST_CASE("Auto-detect - a JSON path is refused") {
    {
        std::ofstream f("test.json");
        f << R"([{"val": 42}])";
    }

    Database db;
    Connection conn(db);
    CheckRefused(conn, "SELECT * FROM 'test.json'");
    CheckRefused(conn, "DESCRIBE read_json('test.json')");

    cleanup_files();
}

// ============================================================================
// COPY with FORMAT JSON: refused
// ============================================================================

TEST_CASE("COPY TO JSON format is refused and writes nothing") {
    cleanup_files();
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t (id INTEGER, name VARCHAR)");
    conn.Query("INSERT INTO t VALUES (1, 'Alice'), (2, 'Bob')");

    CheckRefused(conn, "COPY t TO 'test_out.json' WITH (FORMAT JSON)");
    CheckRefused(conn, "COPY t TO 'test_out.json'");
    CHECK_FALSE(std::filesystem::exists("test_out.json"));

    cleanup_files();
}

TEST_CASE("COPY FROM JSON format is refused and loads nothing") {
    {
        std::ofstream f("test.json");
        f << R"([{"id": 10, "name": "X"}, {"id": 20, "name": "Y"}])";
    }

    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t (id INTEGER, name VARCHAR)");
    CheckRefused(conn, "COPY t FROM 'test.json' WITH (FORMAT JSON)");
    CheckRefused(conn, "COPY t FROM 'test.json'");

    auto r = conn.Query("SELECT COUNT(*) FROM t");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 0);

    cleanup_files();
}
