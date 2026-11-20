// Reading or writing files from SQL is refused (libzugodb Stage 2, #4).
//
// Every route from SQL to a file is refused before a byte is read: the
// paths below do not exist (their directory does not), so an error that
// is not the refusal ("file not found", "no files matching") means the
// route reached a reader. After each refusal the connection answers. The
// readers themselves stay tested through their C++ API until Stage 11
// removes them.

#include "doctest.h"
#include "../refused_files.hpp"
#include "slothdb/catalog/catalog.hpp"
#include "slothdb/main/connection.hpp"
#include "slothdb/main/database.hpp"

#include <filesystem>
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
