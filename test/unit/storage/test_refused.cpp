// Reading or writing files from SQL is refused (libzugodb Stage 2, #4).
//
// Every route from SQL to a file is refused before a byte is read: the
// paths below do not exist (their directory does not), so an error that
// is not the refusal ("file not found", "no files matching") means the
// route reached a reader. After each refusal the connection answers. The
// readers are removed since Stage 11 (#16); the refusals stay.

#include "doctest.h"
#include "../refused_files.hpp"
#include "zugodb/catalog/catalog.hpp"
#include "zugodb/main/connection.hpp"
#include "zugodb/main/database.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace zugodb;
using zugodb::testing::CheckRefused;

namespace {

// A directory that does not exist, so nothing under it can be read.
std::string Absent(const std::string &name) {
    auto dir = fs::temp_directory_path() / "zugodb_refused_absent_dir";
    std::error_code ec;
    fs::remove_all(dir, ec);
    return (dir / name).generic_string();
}

// A path in a directory that exists, where COPY TO could write.
std::string Writable(const std::string &name) {
    auto p = fs::temp_directory_path() / ("zugodb_refused_" + name);
    std::error_code ec;
    fs::remove(p, ec);
    return p.generic_string();
}

std::string Q(const std::string &path) { return "'" + path + "'"; }

// t(x) holds 1, 2, 3; the connection answers the count after a refusal.
void MakeTable(Connection &conn) {
    conn.Query("CREATE TABLE t (x INTEGER)");
    conn.Query("INSERT INTO t VALUES (1), (2), (3)");
}

void CheckTable(Connection &conn) {
    auto r = conn.Query("SELECT count(*), sum(x) FROM t");
    REQUIRE(r.RowCount() == 1);
    CHECK(r.GetValue(0, 0).ToString() == "3");
    CHECK(r.GetValue(0, 1).ToString() == "6");
}

} // namespace

TEST_CASE("Refused - each file table function") {
    Database db;
    Connection conn(db);
    MakeTable(conn);
    const std::vector<std::string> calls = {
        // The ones the engine read before this stage.
        "read_csv(" + Q(Absent("a.csv")) + ")",
        "read_csv_auto(" + Q(Absent("a.csv")) + ")",
        "read_parquet(" + Q(Absent("a.parquet")) + ")",
        "read_json(" + Q(Absent("a.json")) + ")",
        "read_json_auto(" + Q(Absent("a.json")) + ")",
        "read_arrow(" + Q(Absent("a.arrow")) + ")",
        "read_avro(" + Q(Absent("a.avro")) + ")",
        "read_xlsx(" + Q(Absent("a.xlsx")) + ")",
        "sqlite_scan(" + Q(Absent("a.sqlite")) + ", 't')",
        // DuckDB's other file table functions, refused the same way.
        "read_ndjson(" + Q(Absent("a.ndjson")) + ")",
        "read_ndjson_auto(" + Q(Absent("a.ndjson")) + ")",
        "read_json_objects(" + Q(Absent("a.json")) + ")",
        "read_ndjson_objects(" + Q(Absent("a.ndjson")) + ")",
        "parquet_scan(" + Q(Absent("a.parquet")) + ")",
        "parquet_metadata(" + Q(Absent("a.parquet")) + ")",
        "parquet_schema(" + Q(Absent("a.parquet")) + ")",
        "read_text(" + Q(Absent("a.txt")) + ")",
        "read_blob(" + Q(Absent("a.bin")) + ")",
        "sniff_csv(" + Q(Absent("a.csv")) + ")",
        "glob(" + Q(Absent("*.csv")) + ")",
        // Case, options, globs, lists and hive partitioning.
        "READ_CSV(" + Q(Absent("a.csv")) + ")",
        "Read_Parquet(" + Q(Absent("a.parquet")) + ")",
        "read_csv(" + Q(Absent("a.csv")) + ", header = true, delim = ',')",
        "read_csv(" + Q(Absent("*.csv")) + ")",
        "read_parquet(" + Q(Absent("*/*.parquet")) + ", hive_partitioning = true)",
        "read_parquet([" + Q(Absent("a.parquet")) + ", " + Q(Absent("b.parquet")) + "])",
        "read_json(" + Q(Absent("a.json")) + ", format = 'newline_delimited')",
    };
    for (const auto &call : calls) {
        CheckRefused(conn, "SELECT * FROM " + call);
        CheckTable(conn);
    }
}

TEST_CASE("Refused - a file path used as a table, for each extension") {
    Database db;
    Connection conn(db);
    MakeTable(conn);
    for (const char *name :
         {"a.csv", "a.tsv", "a.txt", "a.csv.gz", "a.parquet", "a.json", "a.ndjson", "a.jsonl",
          "a.arrow", "a.feather", "a.ipc", "a.avro", "a.xlsx", "a.xls", "a.sqlite", "a.sqlite3",
          "a.db", "a.unknown", "no_extension", "*.csv", "*.parquet", "dir/*/*.parquet",
          "a?.json"}) {
        CheckRefused(conn, "SELECT * FROM " + Q(Absent(name)));
        CheckTable(conn);
    }
    // Remote paths were refused before; they meet the same refusal first.
    for (const char *url : {"https://example.com/a.csv", "http://example.com/a.parquet",
                            "s3://bucket/a.json"}) {
        CheckRefused(conn, "SELECT * FROM " + Q(url));
        CheckRefused(conn, std::string("SELECT * FROM read_csv('") + url + "')");
        CheckTable(conn);
    }
}

TEST_CASE("Refused - a file anywhere in a statement") {
    Database db;
    Connection conn(db);
    MakeTable(conn);
    const auto csv = Q(Absent("a.csv"));
    const auto rc = "read_csv(" + csv + ")";
    const std::vector<std::string> sqls = {
        "SELECT * FROM " + csv + " AS f(a, b)",
        "SELECT * FROM (" + csv + ")",
        "SELECT * FROM (SELECT * FROM " + csv + ") s",
        "SELECT * FROM t JOIN " + csv + " f ON t.x = f.x",
        "SELECT * FROM t, " + rc,
        "SELECT x FROM t UNION ALL SELECT * FROM " + rc,
        "WITH c AS (SELECT * FROM " + csv + ") SELECT * FROM c",
        "SELECT * FROM t WHERE x IN (SELECT * FROM " + rc + ")",
        "SELECT * FROM t WHERE EXISTS (SELECT * FROM " + csv + ")",
        "SELECT (SELECT count(*) FROM " + rc + ")",
        "INSERT INTO t SELECT * FROM " + csv,
        "CREATE TABLE u AS SELECT * FROM " + rc,
        "CREATE VIEW v AS SELECT * FROM " + csv,
        "CREATE LIVE VIEW lv AS SELECT * FROM " + csv,
        "CREATE LIVE VIEW lv AS SELECT 1 AS x",
        "EXPLAIN SELECT * FROM " + rc,
        "DESCRIBE " + csv,
        "DESC " + Q(Absent("a.parquet")),
        "DESCRIBE " + rc,
        "DESCRIBE read_parquet(" + Q(Absent("a.parquet")) + ")",
        "DESCRIBE SELECT * FROM " + csv,
    };
    for (const auto &sql : sqls) {
        CheckRefused(conn, sql);
        CheckTable(conn);
    }
    CHECK(db.GetCatalog().GetTable("u") == nullptr);
    CHECK(db.GetCatalog().GetTable("v") == nullptr);
    CHECK(db.GetCatalog().GetTable("lv") == nullptr);
}

TEST_CASE("Refused - COPY from a file, for each format") {
    Database db;
    Connection conn(db);
    MakeTable(conn);
    for (const std::string &sql : {
             "COPY t FROM " + Q(Absent("a.csv")),
             "COPY t FROM " + Q(Absent("a.tsv")) + " WITH (DELIMITER '\t', HEADER true)",
             "COPY t FROM " + Q(Absent("a.parquet")),
             "COPY t FROM " + Q(Absent("a.dat")) + " WITH (FORMAT PARQUET)",
             "COPY t FROM " + Q(Absent("a.json")),
             "COPY t FROM " + Q(Absent("a.ndjson")),
             "COPY t FROM " + Q(Absent("a.dat")) + " WITH (FORMAT JSON)",
             "COPY t FROM " + Q(Absent("*.csv")),
         }) {
        CheckRefused(conn, sql);
        CheckTable(conn);
    }
}

TEST_CASE("Refused - COPY to a file, for each format, writes nothing") {
    // The directory exists, so a COPY that reached a writer would create
    // the file. COPY t TO '*.parquet' also returned a wrong answer when
    // read back (GROUP BY found no rows, #4).
    Database db;
    Connection conn(db);
    MakeTable(conn);
    struct Case { std::string file, with; };
    for (const Case &c : std::vector<Case>{
             {"out.csv", ""},
             {"out.tsv", " WITH (DELIMITER '\t', HEADER false)"},
             {"out.parquet", ""},
             {"out.dat", " WITH (FORMAT PARQUET)"},
             {"out.json", ""},
             {"out.ndjson", ""},
             {"out2.dat", " WITH (FORMAT JSON)"},
         }) {
        const auto path = Writable(c.file);
        CheckRefused(conn, "COPY t TO " + Q(path) + c.with);
        CHECK_FALSE(fs::exists(path));
        CheckRefused(conn, "COPY (SELECT x * 2 AS y FROM t) TO " + Q(path) + c.with);
        CHECK_FALSE(fs::exists(path));
        CheckTable(conn);
    }
}

TEST_CASE("Refused - a batch with a file route runs none of its statements") {
    Database db;
    Connection conn(db);
    MakeTable(conn);
    CheckRefused(conn, "CREATE TABLE u (a INTEGER); INSERT INTO t VALUES (4); "
                       "SELECT * FROM " + Q(Absent("a.csv")));
    CHECK(db.GetCatalog().GetTable("u") == nullptr);
    CheckTable(conn);
}

TEST_CASE("Refused - names that are not file routes still work") {
    // A table may be called read_csv, and a string literal is still a value.
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE read_csv (path VARCHAR)");
    conn.Query("INSERT INTO read_csv VALUES ('a.csv')");
    auto r = conn.Query("SELECT path, 'b.parquet' AS other FROM read_csv WHERE path = 'a.csv'");
    REQUIRE(r.RowCount() == 1);
    CHECK(r.GetValue(0, 0).ToString() == "a.csv");
    CHECK(r.GetValue(0, 1).ToString() == "b.parquet");
    auto d = conn.Query("DESCRIBE read_csv");
    CHECK(d.RowCount() == 1);
}

// ============================================================================
// Over files that exist (moved from the reader tests in Stage 11, #16)
// ============================================================================
//
// The readers are gone; these cases, which were in test_csv.cpp,
// test_json_fileio.cpp, test_sqlite.cpp, test_arrow.cpp and
// test_parquet.cpp, give SQL a file that exists, and check that it is still
// refused and that nothing is read, loaded or written. The Parquet files
// are now the format's magic bytes, not a file the removed writer made.

namespace {

const char *TEST_CSV = "test_data.csv";
const char *TEST_OUT_CSV = "test_output.csv";
const char *TEST_PARQUET = "test_data.parquet";

void cleanup_csv() {
    std::remove(TEST_CSV);
    std::remove(TEST_OUT_CSV);
}

void write_test_csv() {
    std::ofstream f(TEST_CSV);
    f << "id,name,score\n";
    f << "1,Alice,95.5\n";
    f << "2,Bob,87.3\n";
    f << "3,Charlie,92.1\n";
    f << "4,\"Diana, Jr.\",88.0\n";
    f.close();
}

void cleanup_json() {
    std::remove("test.json");
    std::remove("test.ndjson");
    std::remove("test_out.json");
    std::remove("test_data2.csv");
}

std::string tmp_file(const std::string &name, const char *bytes, size_t n) {
    auto p = fs::temp_directory_path() / ("zugodb_refused_" + name);
    std::ofstream f(p, std::ios::binary);
    f.write(bytes, static_cast<std::streamsize>(n));
    return p.string();
}

fs::path tmp_arrow(const std::string &tag, const std::string &ext = ".arrow") {
    auto p = fs::temp_directory_path() / ("zugodb_arrow_" + tag + ext);
    std::error_code ec; fs::remove(p, ec);
    // A file that starts as an Arrow IPC file does.
    std::ofstream f(p, std::ios::binary);
    f.write("ARROW1\0\0", 8);
    return p;
}

void cleanup_parquet() { std::remove(TEST_PARQUET); }

// A file that starts and ends as a Parquet file does.
void write_parquet_magic() {
    std::ofstream f(TEST_PARQUET, std::ios::binary);
    f.write("PAR1\0\0\0\0PAR1", 12);
}

} // namespace

TEST_CASE("COPY TO a CSV file is refused and writes nothing") {
    cleanup_csv();
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t (id INTEGER, name VARCHAR)");
    conn.Query("INSERT INTO t VALUES (1, 'Alice'), (2, 'Bob')");

    CheckRefused(conn, "COPY t TO 'test_output.csv'");
    CHECK_FALSE(std::filesystem::exists(TEST_OUT_CSV));

    cleanup_csv();
}

TEST_CASE("COPY FROM a CSV file is refused and loads nothing") {
    write_test_csv();
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE imported (id INTEGER, name VARCHAR, score DOUBLE)");

    CheckRefused(conn, "COPY imported FROM 'test_data.csv'");

    auto r = conn.Query("SELECT COUNT(*) FROM imported");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 0);

    cleanup_csv();
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

    cleanup_csv();
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

TEST_CASE("read_json - refused over a JSON file") {
    {
        std::ofstream f("test.json");
        f << R"([{"id": 1, "name": "Alice"}, {"id": 2, "name": "Bob"}])";
    }

    Database db;
    Connection conn(db);
    CheckRefused(conn, "SELECT * FROM read_json('test.json')");
    CheckRefused(conn, "SELECT * FROM read_json_auto('test.json')");

    cleanup_json();
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

    cleanup_json();
}

TEST_CASE("read_csv - refused over a CSV file") {
    {
        std::ofstream f("test_data2.csv");
        f << "a,b\n1,hello\n2,world\n";
    }

    Database db;
    Connection conn(db);
    CheckRefused(conn, "SELECT * FROM read_csv('test_data2.csv')");
    CheckRefused(conn, "SELECT * FROM read_csv_auto('test_data2.csv')");

    cleanup_json();
}

TEST_CASE("Auto-detect - a CSV path is refused") {
    {
        std::ofstream f("test_data2.csv");
        f << "x,y\n10,hello\n20,world\n";
    }

    Database db;
    Connection conn(db);
    CheckRefused(conn, "SELECT * FROM 'test_data2.csv'");
    CheckRefused(conn, "DESCRIBE 'test_data2.csv'");

    cleanup_json();
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

    cleanup_json();
}

TEST_CASE("COPY TO JSON format is refused and writes nothing") {
    cleanup_json();
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t (id INTEGER, name VARCHAR)");
    conn.Query("INSERT INTO t VALUES (1, 'Alice'), (2, 'Bob')");

    CheckRefused(conn, "COPY t TO 'test_out.json' WITH (FORMAT JSON)");
    CheckRefused(conn, "COPY t TO 'test_out.json'");
    CHECK_FALSE(std::filesystem::exists("test_out.json"));

    cleanup_json();
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

    cleanup_json();
}

TEST_CASE("sqlite_scan(), read_xlsx() and read_avro() are refused") {
    auto sq = tmp_file("b.sqlite", "SQLite format 3\0", 16);
    auto xl = tmp_file("b.xlsx", "PK\3\4", 4);
    auto av = tmp_file("b.avro", "Obj\1", 4);
    Database db;
    Connection conn(db);
    CheckRefused(conn, "SELECT * FROM sqlite_scan('" + sq + "', 't')");
    CheckRefused(conn, "SELECT * FROM read_xlsx('" + xl + "')");
    CheckRefused(conn, "SELECT * FROM read_avro('" + av + "')");
}

TEST_CASE("A .sqlite, .db, .xlsx, .xls or .avro path is refused") {
    Database db;
    Connection conn(db);
    for (const char *ext : {".sqlite", ".sqlite3", ".db", ".xlsx", ".xls", ".avro"}) {
        auto p = tmp_file(std::string("c") + ext, "x", 1);
        CheckRefused(conn, "SELECT * FROM '" + p + "'");
    }
}

TEST_CASE("read_arrow() is refused") {
    auto path = tmp_arrow("sql").string();
    Database db;
    Connection conn(db);
    CheckRefused(conn, "SELECT * FROM read_arrow('" + path + "')");
}

TEST_CASE("A .arrow, .feather or .ipc path is refused") {
    Database db;
    Connection conn(db);
    for (const char *ext : {".arrow", ".feather", ".ipc"}) {
        auto path = tmp_arrow("path", ext).string();
        CheckRefused(conn, "SELECT * FROM '" + path + "'");
    }
}

TEST_CASE("Parquet - COPY TO and FROM are refused") {
    cleanup_parquet();
    {
        write_parquet_magic();
    }

    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t (id INTEGER, name VARCHAR, val DOUBLE)");
    conn.Query("INSERT INTO t VALUES (1, 'A', 1.1), (2, 'B', 2.2), (3, 'C', 3.3)");

    const char *out = "test_data_out.parquet";
    std::remove(out);
    CheckRefused(conn, std::string("COPY t TO '") + out + "' WITH (FORMAT PARQUET)");
    CheckRefused(conn, std::string("COPY t TO '") + out + "'");
    CHECK_FALSE(std::filesystem::exists(out));

    conn.Query("CREATE TABLE t2 (id INTEGER)");
    CheckRefused(conn, "COPY t2 FROM 'test_data.parquet' WITH (FORMAT PARQUET)");
    CheckRefused(conn, "COPY t2 FROM 'test_data.parquet'");
    auto r = conn.Query("SELECT COUNT(*) FROM t2");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 0);

    cleanup_parquet();
}

TEST_CASE("Parquet - read_parquet() and a .parquet path are refused") {
    cleanup_parquet();

    {
        write_parquet_magic();
    }

    // The file exists; SQL still may not name it.
    Database db;
    Connection conn(db);
    CheckRefused(conn, "SELECT * FROM 'test_data.parquet'");
    CheckRefused(conn, "SELECT * FROM read_parquet('test_data.parquet')");
    CheckRefused(conn, "DESCRIBE 'test_data.parquet'");

    cleanup_parquet();
}
