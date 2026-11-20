// zuslothdb (pedrobtz/zuslothdb#40): SlothDB's Arrow reader and writer used a
// format of their own after Arrow's magic bytes, not Arrow IPC, so they could
// read no file from another Arrow implementation and wrote files none could
// read. Both are refused; these cases replace the round trips of that format.
// From SQL, read_arrow() and an Arrow path now meet the refusal of every
// file route (Stage 2, #4) first.

#include "doctest.h"
#include "../refused_files.hpp"
#include "slothdb/main/connection.hpp"
#include "slothdb/main/database.hpp"
#include "slothdb/storage/arrow_ipc.hpp"
#include "slothdb/common/exception.hpp"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace zugodb;
using zugodb::testing::CheckRefused;

static fs::path tmp_arrow(const std::string &tag, const std::string &ext = ".arrow") {
    auto p = fs::temp_directory_path() / ("slothdb_arrow_" + tag + ext);
    std::error_code ec; fs::remove(p, ec);
    // A file that starts as an Arrow IPC file does.
    std::ofstream f(p, std::ios::binary);
    f.write("ARROW1\0\0", 8);
    return p;
}

TEST_CASE("ArrowIPCWriter is refused") {
    auto path = tmp_arrow("writer").string();
    CHECK_THROWS_AS(ArrowIPCWriter(path, {"id"}, {LogicalType::INTEGER()}),
                    NotImplementedException);
}

TEST_CASE("ArrowIPCReader is refused") {
    auto path = tmp_arrow("reader").string();
    CHECK_THROWS_AS(ArrowIPCReader{path}, NotImplementedException);
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
