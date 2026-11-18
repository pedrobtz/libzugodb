// zuslothdb (pedrobtz/zuslothdb#55): SlothDB's SQLite, Excel and Avro
// readers are refused, the first two because they returned wrong values
// against real files, Avro because it is untested; these cases replace the
// SQLite round trips.

#include "doctest.h"
#include "slothdb/main/connection.hpp"
#include "slothdb/main/database.hpp"
#include "slothdb/storage/sqlite_scanner.hpp"
#include "slothdb/storage/excel_reader.hpp"
#include "slothdb/storage/avro_reader.hpp"
#include "slothdb/common/exception.hpp"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace zugodb;

static std::string tmp_file(const std::string &name, const char *bytes, size_t n) {
    auto p = fs::temp_directory_path() / ("slothdb_refused_" + name);
    std::ofstream f(p, std::ios::binary);
    f.write(bytes, static_cast<std::streamsize>(n));
    return p.string();
}

TEST_CASE("SQLiteScanner, ExcelReader and AvroReader are refused") {
    auto sq = tmp_file("a.sqlite", "SQLite format 3\0", 16);
    auto xl = tmp_file("a.xlsx", "PK\3\4", 4);
    auto av = tmp_file("a.avro", "Obj\1", 4);
    CHECK_THROWS_AS(SQLiteScanner{sq}, NotImplementedException);
    CHECK_THROWS_AS(ExcelReader(xl, ""), NotImplementedException);
    CHECK_THROWS_AS(AvroReader{av}, NotImplementedException);
}

TEST_CASE("sqlite_scan(), read_xlsx() and read_avro() are refused") {
    auto sq = tmp_file("b.sqlite", "SQLite format 3\0", 16);
    auto xl = tmp_file("b.xlsx", "PK\3\4", 4);
    auto av = tmp_file("b.avro", "Obj\1", 4);
    Database db;
    Connection conn(db);
    CHECK_THROWS_AS(conn.Query("SELECT * FROM sqlite_scan('" + sq + "', 't')"),
                    NotImplementedException);
    CHECK_THROWS_AS(conn.Query("SELECT * FROM read_xlsx('" + xl + "')"),
                    NotImplementedException);
    CHECK_THROWS_AS(conn.Query("SELECT * FROM read_avro('" + av + "')"),
                    NotImplementedException);
}

TEST_CASE("A .sqlite, .db, .xlsx, .xls or .avro path is refused") {
    Database db;
    Connection conn(db);
    for (const char *ext : {".sqlite", ".sqlite3", ".db", ".xlsx", ".xls", ".avro"}) {
        auto p = tmp_file(std::string("c") + ext, "x", 1);
        CHECK_THROWS_AS(conn.Query("SELECT * FROM '" + p + "'"), NotImplementedException);
    }
}
