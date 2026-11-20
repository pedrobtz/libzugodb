// The CSV reader and writer through their C++ API. SQL cannot read or write
// a file since Stage 2 (#4); the cases that did so check that it is refused,
// over files that exist.

#include "doctest.h"
#include "../refused_files.hpp"
#include "slothdb/main/database.hpp"
#include "slothdb/catalog/catalog.hpp"
#include "slothdb/main/connection.hpp"
#include "slothdb/storage/csv_reader.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace zugodb;
using zugodb::testing::CheckRefused;

static const char *TEST_CSV = "test_data.csv";
static const char *TEST_OUT_CSV = "test_output.csv";

static void cleanup() {
    std::remove(TEST_CSV);
    std::remove(TEST_OUT_CSV);
}

static void write_test_csv() {
    std::ofstream f(TEST_CSV);
    f << "id,name,score\n";
    f << "1,Alice,95.5\n";
    f << "2,Bob,87.3\n";
    f << "3,Charlie,92.1\n";
    f << "4,\"Diana, Jr.\",88.0\n";
    f.close();
}

TEST_CASE("CSV Reader - basic read") {
    write_test_csv();

    CSVReader reader(TEST_CSV);
    auto header = reader.ReadHeader();
    CHECK(header.size() == 3);
    CHECK(header[0] == "id");
    CHECK(header[1] == "name");
    CHECK(header[2] == "score");

    auto types = std::vector<LogicalType>{
        LogicalType::INTEGER(), LogicalType::VARCHAR(), LogicalType::DOUBLE()};
    auto rows = reader.ReadAll(types);
    CHECK(rows.size() == 4);
    CHECK(rows[0][0].GetValue<int32_t>() == 1);
    CHECK(rows[0][1].GetValue<std::string>() == "Alice");
    CHECK(rows[0][2].GetValue<double>() == doctest::Approx(95.5));

    // Quoted field with comma.
    CHECK(rows[3][1].GetValue<std::string>() == "Diana, Jr.");

    cleanup();
}

TEST_CASE("CSV Reader - auto-detect types") {
    write_test_csv();

    CSVReader reader(TEST_CSV);
    auto header = reader.ReadHeader();
    auto types = reader.DetectTypes();

    CHECK(types.size() == 3);
    // id should be detected as BIGINT (integer-like).
    CHECK((types[0].id() == LogicalTypeId::BIGINT || types[0].id() == LogicalTypeId::INTEGER));
    // name is VARCHAR.
    CHECK(types[1].id() == LogicalTypeId::VARCHAR);
    // score is DOUBLE or VARCHAR (quoted fields can affect detection).
    CHECK((types[2].id() == LogicalTypeId::DOUBLE ||
           types[2].id() == LogicalTypeId::BIGINT ||
           types[2].id() == LogicalTypeId::VARCHAR));

    cleanup();
}

TEST_CASE("CSV Writer - basic write") {
    CSVWriter writer(TEST_OUT_CSV);
    writer.WriteHeader({"x", "y"});
    writer.WriteRow({Value::INTEGER(1), Value::VARCHAR("hello")});
    writer.WriteRow({Value::INTEGER(2), Value::VARCHAR("world")});
    writer.Flush();

    // Read it back.
    CSVReader reader(TEST_OUT_CSV);
    auto header = reader.ReadHeader();
    CHECK(header.size() == 2);
    CHECK(header[0] == "x");

    auto types = std::vector<LogicalType>{LogicalType::INTEGER(), LogicalType::VARCHAR()};
    auto rows = reader.ReadAll(types);
    CHECK(rows.size() == 2);
    CHECK(rows[0][0].GetValue<int32_t>() == 1);
    CHECK(rows[1][1].GetValue<std::string>() == "world");

    cleanup();
}

TEST_CASE("COPY TO a CSV file is refused and writes nothing") {
    cleanup();
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t (id INTEGER, name VARCHAR)");
    conn.Query("INSERT INTO t VALUES (1, 'Alice'), (2, 'Bob')");

    CheckRefused(conn, "COPY t TO 'test_output.csv'");
    CHECK_FALSE(std::filesystem::exists(TEST_OUT_CSV));

    cleanup();
}

TEST_CASE("COPY FROM a CSV file is refused and loads nothing") {
    write_test_csv();
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE imported (id INTEGER, name VARCHAR, score DOUBLE)");

    CheckRefused(conn, "COPY imported FROM 'test_data.csv'");

    auto r = conn.Query("SELECT COUNT(*) FROM imported");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 0);

    cleanup();
}

TEST_CASE("COPY with options is refused") {
    write_test_csv();
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE original (x INTEGER, y VARCHAR, z DOUBLE)");
    conn.Query("INSERT INTO original VALUES (1, 'hello', 3.14), (2, 'world', 2.72)");

    CheckRefused(conn, "COPY original TO 'test_output.csv' WITH (DELIMITER '|', HEADER true)");
    CHECK_FALSE(std::filesystem::exists(TEST_OUT_CSV));
    CheckRefused(conn, "COPY original FROM 'test_data.csv' WITH (DELIMITER ',', HEADER true)");
    auto r = conn.Query("SELECT COUNT(*) FROM original");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 2);

    cleanup();
}

TEST_CASE("File-literal JOIN across two CSVs is refused") {
    // Both files exist; neither side of the JOIN may name one, and a
    // refused query leaves no table behind in the catalog.
    std::ofstream u("test_users.csv");
    u << "id,name\n1,Alice\n2,Bob\n3,Carol\n";
    u.close();
    std::ofstream o("test_orders.csv");
    o << "user_id,amount\n1,100\n1,250\n2,50\n";
    o.close();

    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE users (id INTEGER, name VARCHAR)");
    for (int i = 0; i < 3; i++) {
        CheckRefused(conn,
            "SELECT u.name, o.amount FROM 'test_users.csv' u "
            "INNER JOIN 'test_orders.csv' o ON u.id = o.user_id");
        CheckRefused(conn,
            "SELECT u.id FROM users u "
            "LEFT JOIN 'test_orders.csv' o ON u.id = o.user_id");
    }
    CHECK(db.GetCatalog().GetSchema().GetTableNames().size() == 1);

    std::remove("test_users.csv");
    std::remove("test_orders.csv");
}

TEST_CASE("COPY with a file-literal source is refused") {
    std::ofstream in("test_src.csv");
    in << "id,val\n1,a\n2,b\n3,c\n";
    in.close();
    std::remove("test_copy_out.csv");

    Database db;
    Connection conn(db);
    CheckRefused(conn,
        "COPY (SELECT id, val FROM 'test_src.csv' WHERE id > 1) "
        "TO 'test_copy_out.csv'");
    CHECK_FALSE(std::filesystem::exists("test_copy_out.csv"));

    std::remove("test_src.csv");
}
