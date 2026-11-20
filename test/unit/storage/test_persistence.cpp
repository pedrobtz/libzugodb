#include "doctest.h"
#include "slothdb/main/database.hpp"
#include "slothdb/main/connection.hpp"
#include "slothdb/storage/checkpoint.hpp"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace zugodb;

static const char *TEST_DB_PATH = "test_slothdb.db";

// Clean up test file.
static void cleanup() {
    std::remove(TEST_DB_PATH);
}

TEST_CASE("Persistence - save and load basic table") {
    cleanup();

    // Create DB, insert data, close (triggers save).
    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        conn.Query("CREATE TABLE users (id INTEGER, name VARCHAR, score DOUBLE)");
        conn.Query("INSERT INTO users VALUES (1, 'Alice', 95.5)");
        conn.Query("INSERT INTO users VALUES (2, 'Bob', 87.3)");
        conn.Query("INSERT INTO users VALUES (3, 'Charlie', 92.1)");
    } // Database destructor saves to file.

    // Reopen and verify data persists.
    {
        Database db(TEST_DB_PATH);
        Connection conn(db);

        auto r = conn.Query("SELECT * FROM users");
        CHECK(r.RowCount() == 3);
        CHECK(r.ColumnCount() == 3);

        // Verify values.
        bool found_alice = false, found_bob = false;
        for (idx_t i = 0; i < r.RowCount(); i++) {
            auto name = r.GetValue(i, 1).GetValue<std::string>();
            if (name == "Alice") {
                CHECK(r.GetValue(i, 0).GetValue<int32_t>() == 1);
                CHECK(r.GetValue(i, 2).GetValue<double>() == doctest::Approx(95.5));
                found_alice = true;
            }
            if (name == "Bob") {
                CHECK(r.GetValue(i, 0).GetValue<int32_t>() == 2);
                found_bob = true;
            }
        }
        CHECK(found_alice);
        CHECK(found_bob);
    }

    cleanup();
}

TEST_CASE("Persistence - multiple tables") {
    cleanup();

    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        conn.Query("CREATE TABLE t1 (x INTEGER)");
        conn.Query("INSERT INTO t1 VALUES (10), (20), (30)");
        conn.Query("CREATE TABLE t2 (name VARCHAR, active INTEGER)");
        conn.Query("INSERT INTO t2 VALUES ('foo', 1), ('bar', 0)");
    }

    {
        Database db(TEST_DB_PATH);
        Connection conn(db);

        auto r1 = conn.Query("SELECT COUNT(*) FROM t1");
        CHECK(r1.GetValue(0, 0).GetValue<int64_t>() == 3);

        auto r2 = conn.Query("SELECT COUNT(*) FROM t2");
        CHECK(r2.GetValue(0, 0).GetValue<int64_t>() == 2);

        auto r3 = conn.Query("SELECT name FROM t2 WHERE active = 1");
        CHECK(r3.RowCount() == 1);
        CHECK(r3.GetValue(0, 0).GetValue<std::string>() == "foo");
    }

    cleanup();
}

TEST_CASE("Persistence - null values") {
    cleanup();

    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        conn.Query("CREATE TABLE t (x INTEGER, y VARCHAR)");
        conn.Query("INSERT INTO t VALUES (1, 'hello')");
        conn.Query("INSERT INTO t VALUES (NULL, 'world')");
        conn.Query("INSERT INTO t VALUES (3, NULL)");
    }

    {
        Database db(TEST_DB_PATH);
        Connection conn(db);

        auto r = conn.Query("SELECT * FROM t");
        CHECK(r.RowCount() == 3);
        CHECK_FALSE(r.GetValue(0, 0).IsNull());
        CHECK(r.GetValue(1, 0).IsNull());
        CHECK_FALSE(r.GetValue(2, 0).IsNull());
        CHECK(r.GetValue(2, 1).IsNull());
    }

    cleanup();
}

TEST_CASE("Persistence - modify and re-save") {
    cleanup();

    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        conn.Query("CREATE TABLE t (x INTEGER)");
        conn.Query("INSERT INTO t VALUES (1), (2), (3)");
    }

    // Reopen, modify, close again.
    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        conn.Query("INSERT INTO t VALUES (4), (5)");
        conn.Query("DELETE FROM t WHERE x = 1");
    }

    // Verify final state.
    {
        Database db(TEST_DB_PATH);
        Connection conn(db);

        auto r = conn.Query("SELECT COUNT(*) FROM t");
        CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 4); // 2,3,4,5
    }

    cleanup();
}

TEST_CASE("Persistence - in-memory mode (no path)") {
    // Empty path = in-memory only, no persistence.
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t (x INTEGER)");
    conn.Query("INSERT INTO t VALUES (42)");

    auto r = conn.Query("SELECT x FROM t");
    CHECK(r.GetValue(0, 0).GetValue<int32_t>() == 42);
    // No file created, no crash on destruct.
}

TEST_CASE("Persistence - query after load") {
    cleanup();

    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        conn.Query("CREATE TABLE sales (product VARCHAR, amount INTEGER)");
        conn.Query("INSERT INTO sales VALUES ('A', 100), ('B', 200), ('A', 150)");
    }

    {
        Database db(TEST_DB_PATH);
        Connection conn(db);

        // Run aggregation on loaded data.
        auto r = conn.Query(
            "SELECT product, SUM(amount) FROM sales GROUP BY product");
        CHECK(r.RowCount() == 2);

        for (idx_t i = 0; i < r.RowCount(); i++) {
            auto product = r.GetValue(i, 0).GetValue<std::string>();
            auto total = r.GetValue(i, 1).GetValue<int64_t>();
            if (product == "A") CHECK(total == 250);
            if (product == "B") CHECK(total == 200);
        }
    }

    cleanup();
}

// ============================================================================
// Format 1001: the checksum, the version and the golden file (Stage 3)
// ============================================================================

namespace {

const std::string FIXTURES = std::string(ZUGODB_SOURCE_DIR) + "/test/fixtures/";
const char *DAMAGED_DB_PATH = "test_damaged.db";

std::vector<uint8_t> ReadBytes(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

void WriteBytes(const std::string &path, const std::vector<uint8_t> &bytes, size_t n) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(n));
    REQUIRE(out.good());
}

// Opens `path`, which must be refused; returns the message. The file must
// be left as it was: a refused open does not save over it.
std::string OpenRefused(const std::string &path) {
    auto before = ReadBytes(path);
    std::string message;
    try {
        Database db(path);
    } catch (const std::exception &e) {
        message = e.what();
    }
    CHECK(ReadBytes(path) == before);
    return message;
}

// A copy of a committed fixture, opened read-only so it is never saved.
DatabaseConfig ReadOnlyCopy(const std::string &fixture) {
    auto bytes = ReadBytes(FIXTURES + fixture);
    WriteBytes(DAMAGED_DB_PATH, bytes, bytes.size());
    DatabaseConfig config;
    config.path = DAMAGED_DB_PATH;
    config.read_only = true;
    return config;
}

} // namespace

TEST_CASE("Persistence - the golden file reads back its recorded rows") {
    // test/fixtures/golden_v1001.db: tools/db_fixture.sql written by the
    // slothdb shell at format 1001 (tools/make_db_fixtures.sh). Each value
    // below is the one that SQL inserts.
    std::remove(DAMAGED_DB_PATH);
    {
        Database db(ReadOnlyCopy("golden_v1001.db"));
        Connection conn(db);

        // DATE as days and TIMESTAMP as microseconds since 1970-01-01, as
        // Python's datetime computes them from the literals.
        auto r = conn.Query("SELECT id, i8, i16, i64, f64, s, b, d, ts FROM golden ORDER BY id");
        REQUIRE(r.RowCount() == 4);
        REQUIRE(r.ColumnCount() == 9);
        CHECK(r.column_types[1].id() == LogicalTypeId::TINYINT);
        CHECK(r.column_types[2].id() == LogicalTypeId::SMALLINT);
        CHECK(r.column_types[3].id() == LogicalTypeId::BIGINT);
        CHECK(r.column_types[4].id() == LogicalTypeId::DOUBLE);
        CHECK(r.column_types[6].id() == LogicalTypeId::BOOLEAN);
        CHECK(r.column_types[7].id() == LogicalTypeId::DATE);
        CHECK(r.column_types[8].id() == LogicalTypeId::TIMESTAMP);

        CHECK(r.GetValue(0, 0).GetValue<int32_t>() == 1);
        CHECK(r.GetValue(0, 1).GetValue<int8_t>() == -128);
        CHECK(r.GetValue(0, 2).GetValue<int16_t>() == -32768);
        CHECK(r.GetValue(0, 3).GetValue<int64_t>() == -9223372036854775807LL);
        CHECK(r.GetValue(0, 4).GetValue<double>() == -0.5);
        CHECK(r.GetValue(0, 5).GetValue<std::string>() == "zugodb");
        CHECK(r.GetValue(0, 6).GetValue<bool>() == true);
        CHECK(r.GetValue(0, 7).GetValue<int32_t>() == 0);
        CHECK(r.GetValue(0, 8).GetValue<int64_t>() == 0);

        CHECK(r.GetValue(1, 0).GetValue<int32_t>() == 2);
        CHECK(r.GetValue(1, 1).GetValue<int8_t>() == 127);
        CHECK(r.GetValue(1, 2).GetValue<int16_t>() == 32767);
        CHECK(r.GetValue(1, 3).GetValue<int64_t>() == 9223372036854775807LL);
        CHECK(r.GetValue(1, 4).GetValue<double>() == 1e300);
        CHECK(r.GetValue(1, 5).GetValue<std::string>() == "h\xC3\xA9llo, w\xC3\xB6rld");
        CHECK(r.GetValue(1, 6).GetValue<bool>() == false);
        CHECK(r.GetValue(1, 7).GetValue<int32_t>() == 19782);
        CHECK(r.GetValue(1, 8).GetValue<int64_t>() == 1709210096789000LL);

        CHECK(r.GetValue(2, 0).GetValue<int32_t>() == 3);
        CHECK(r.GetValue(2, 1).GetValue<int8_t>() == 0);
        CHECK(r.GetValue(2, 4).GetValue<double>() == 0.0);
        CHECK(r.GetValue(2, 5).GetValue<std::string>() == "");
        CHECK(r.GetValue(2, 7).GetValue<int32_t>() == -1);
        CHECK(r.GetValue(2, 8).GetValue<int64_t>() == -1000000);

        CHECK(r.GetValue(3, 0).GetValue<int32_t>() == 4);
        for (idx_t c = 1; c < 9; c++) CHECK(r.GetValue(3, c).IsNull());

        auto e = conn.Query("SELECT COUNT(*) FROM empty_table");
        CHECK(e.GetValue(0, 0).GetValue<int64_t>() == 0);
        CHECK(conn.Query("SELECT x, y FROM empty_table").ColumnCount() == 2);
    }
    std::remove(DAMAGED_DB_PATH);
}

TEST_CASE("Persistence - the stored checksum is ComputeChecksum of the file") {
    // The golden file's checksum, at offset 12, little-endian.
    auto bytes = ReadBytes(FIXTURES + "golden_v1001.db");
    REQUIRE(bytes.size() > 4096);
    uint32_t stored = uint32_t(bytes[12]) | uint32_t(bytes[13]) << 8 |
                      uint32_t(bytes[14]) << 16 | uint32_t(bytes[15]) << 24;
    CHECK(Checkpoint::ComputeChecksum(bytes.data(), bytes.size()) == stored);
    // The stored bytes do not enter it.
    bytes[12] ^= 0xFF;
    CHECK(Checkpoint::ComputeChecksum(bytes.data(), bytes.size()) == stored);
    bytes[0] ^= 0x01;
    CHECK(Checkpoint::ComputeChecksum(bytes.data(), bytes.size()) != stored);
}

TEST_CASE("Persistence - a bit flipped or a truncation anywhere is refused") {
    // A file written now, so the test follows the writer, and the golden
    // file, so it follows what is committed.
    std::remove(TEST_DB_PATH);
    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        conn.Query("CREATE TABLE t (id INTEGER, s VARCHAR, v DOUBLE, d DATE)");
        conn.Query("INSERT INTO t VALUES (1, 'one', 1.5, DATE '2024-01-01'), "
                   "(2, NULL, -2.25, NULL), (3, 'three', NULL, DATE '1999-12-31')");
    }
    for (auto source : {std::string(TEST_DB_PATH), FIXTURES + "golden_v1001.db"}) {
        CAPTURE(source);
        auto good = ReadBytes(source);
        REQUIRE(good.size() > 4096);

        // Every byte of the header, a spread over the zero gap, and every
        // byte of the data, which starts at 4096.
        std::vector<size_t> offsets;
        for (size_t i = 0; i < 64; i++) offsets.push_back(i);
        for (size_t i = 64; i < 4096; i += 257) offsets.push_back(i);
        offsets.push_back(4095);
        for (size_t i = 4096; i < good.size(); i++) offsets.push_back(i);

        for (auto offset : offsets) {
            CAPTURE(offset);
            auto bad = good;
            bad[offset] ^= static_cast<uint8_t>(1u << (offset % 8));
            WriteBytes(DAMAGED_DB_PATH, bad, bad.size());
            CHECK_FALSE(OpenRefused(DAMAGED_DB_PATH).empty());

            WriteBytes(DAMAGED_DB_PATH, good, offset);
            CHECK_FALSE(OpenRefused(DAMAGED_DB_PATH).empty());
        }

        // One byte appended.
        auto longer = good;
        longer.push_back(0);
        WriteBytes(DAMAGED_DB_PATH, longer, longer.size());
        CHECK(OpenRefused(DAMAGED_DB_PATH).find("truncated or extended") != std::string::npos);
    }

    // The causes the messages name.
    auto good = ReadBytes(TEST_DB_PATH);
    WriteBytes(DAMAGED_DB_PATH, good, 47);
    CHECK(OpenRefused(DAMAGED_DB_PATH).find("smaller than its 48-byte header") != std::string::npos);
    WriteBytes(DAMAGED_DB_PATH, good, good.size() - 1);
    CHECK(OpenRefused(DAMAGED_DB_PATH).find("truncated or extended") != std::string::npos);
    auto bad = good;
    bad[4100] ^= 0x10;
    WriteBytes(DAMAGED_DB_PATH, bad, bad.size());
    auto message = OpenRefused(DAMAGED_DB_PATH);
    CHECK(message.find("Damaged database file: checksum 0x") != std::string::npos);
    CHECK(message.find("format version 1001") != std::string::npos);
    bad = good;
    bad[0] ^= 0x01;
    WriteBytes(DAMAGED_DB_PATH, bad, bad.size());
    CHECK(OpenRefused(DAMAGED_DB_PATH).find("bad magic number") != std::string::npos);

    // The original still opens, and a new database works.
    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        CHECK(conn.Query("SELECT SUM(id) FROM t").GetValue(0, 0).GetValue<int64_t>() == 6);
    }
    std::remove(TEST_DB_PATH);
    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        conn.Query("CREATE TABLE fresh (x INTEGER)");
        conn.Query("INSERT INTO fresh VALUES (7)");
    }
    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        CHECK(conn.Query("SELECT x FROM fresh").GetValue(0, 0).GetValue<int32_t>() == 7);
    }
    std::remove(TEST_DB_PATH);
    std::remove(DAMAGED_DB_PATH);
}

TEST_CASE("Persistence - an upstream file of format version 1 is refused") {
    // test/fixtures/format_v1.db: tools/db_fixture.sql written by the
    // slothdb shell at 030f7b8, upstream SlothDB's format (version 1, no
    // checksum): tools/make_db_fixtures.sh v1.
    auto bytes = ReadBytes(FIXTURES + "format_v1.db");
    REQUIRE(bytes.size() > 8);
    CHECK(bytes[4] == 1);
    WriteBytes(DAMAGED_DB_PATH, bytes, bytes.size());
    auto message = OpenRefused(DAMAGED_DB_PATH);
    CHECK(message.find("Unsupported database file format version 1;") != std::string::npos);
    CHECK(message.find("reads version 1001 only") != std::string::npos);
    std::remove(DAMAGED_DB_PATH);
}
