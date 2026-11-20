#include "doctest.h"
#include "slothdb/main/database.hpp"
#include "slothdb/main/connection.hpp"
#include "slothdb/storage/checkpoint.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

TEST_CASE("Persistence - a file of several checksum pieces is summed whole") {
    // Load reads the file in 1 MiB pieces; the stored sum must equal
    // ComputeChecksum over the whole image, and a flip past the first
    // piece must be refused.
    std::remove(TEST_DB_PATH);
    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        conn.Query("CREATE TABLE big (i INTEGER, s VARCHAR)");
        for (int batch = 0; batch < 64; batch++) {
            std::string sql = "INSERT INTO big VALUES ";
            for (int k = 0; k < 1024; k++) {
                auto i = std::to_string(batch * 1024 + k);
                sql += (k ? ", (" : "(") + i + ", 'a row of about thirty bytes " + i + "')";
            }
            conn.Query(sql);
        }
    }
    auto bytes = ReadBytes(TEST_DB_PATH);
    REQUIRE(bytes.size() > 2 * (1u << 20));
    uint32_t stored = uint32_t(bytes[12]) | uint32_t(bytes[13]) << 8 |
                      uint32_t(bytes[14]) << 16 | uint32_t(bytes[15]) << 24;
    CHECK(Checkpoint::ComputeChecksum(bytes.data(), bytes.size()) == stored);
    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        CHECK(conn.Query("SELECT COUNT(*) FROM big").GetValue(0, 0).GetValue<int64_t>() == 65536);
    }
    for (size_t offset : {size_t(1) << 20, (size_t(1) << 20) + 1, bytes.size() - 1}) {
        CAPTURE(offset);
        auto bad = ReadBytes(TEST_DB_PATH);
        bad[offset] ^= 0x04;
        WriteBytes(DAMAGED_DB_PATH, bad, bad.size());
        CHECK(OpenRefused(DAMAGED_DB_PATH).find("checksum") != std::string::npos);
    }
    std::remove(TEST_DB_PATH);
    std::remove(DAMAGED_DB_PATH);
}

// ============================================================================
// Checkpoint::Load bounds every value it reads (Stage 4)
// ============================================================================
//
// Each file below is built by hand: the header Save writes, zeros up to
// 4096, then the bytes of the tables, with catalog_size and the checksum
// set so the file passes BlockManager::OpenFile and the checksum and
// reaches the parser. Each bound is shown at its limit and one past it.

namespace {

const char *HAND_DB_PATH = "test_hand_built.db";

// The tables' bytes, in Save's encoding (little-endian).
struct Bytes {
    std::vector<uint8_t> b;
    Bytes &U8(uint8_t v) { b.push_back(v); return *this; }
    Bytes &U32(uint32_t v) { return Raw(&v, 4); }
    Bytes &U64(uint64_t v) { return Raw(&v, 8); }
    Bytes &I32(int32_t v) { return Raw(&v, 4); }
    Bytes &I64(int64_t v) { return Raw(&v, 8); }
    Bytes &Str(const std::string &s) {
        U32(static_cast<uint32_t>(s.size()));
        return Raw(s.data(), s.size());
    }
    Bytes &Zeros(size_t n) { b.insert(b.end(), n, 0); return *this; }
    Bytes &Raw(const void *p, size_t n) {
        auto *c = static_cast<const uint8_t *>(p);
        b.insert(b.end(), c, c + n);
        return *this;
    }
    // A table of one column ("c") and no rows: 23 bytes and the name's;
    // with both names empty it is 21, the smallest a table can be.
    Bytes &EmptyTable(const std::string &name, uint8_t type = uint8_t(LogicalTypeId::INTEGER)) {
        return Str(name).U32(1).Str("c").U8(type).U64(0);
    }
};

void PutU32(std::vector<uint8_t> &image, size_t at, uint32_t v) { std::memcpy(&image[at], &v, 4); }
void PutU64(std::vector<uint8_t> &image, size_t at, uint64_t v) { std::memcpy(&image[at], &v, 8); }
uint64_t GetU64(const std::vector<uint8_t> &image, size_t at) {
    uint64_t v;
    std::memcpy(&v, &image[at], 8);
    return v;
}

// Sets catalog_size from catalog_offset and the file's size, then the
// checksum, as the fuzz harness does.
void Seal(std::vector<uint8_t> &image) {
    PutU64(image, 24, image.size() - GetU64(image, 16));
    PutU32(image, 12, Checkpoint::ComputeChecksum(image.data(), image.size()));
}

std::vector<uint8_t> Image(const Bytes &tables) {
    FileHeader header;
    header.catalog_offset = FILE_DATA_OFFSET;
    header.data_offset = FILE_DATA_OFFSET;
    std::vector<uint8_t> image(FILE_DATA_OFFSET, 0);
    std::memcpy(image.data(), &header, sizeof(header));
    image.insert(image.end(), tables.b.begin(), tables.b.end());
    Seal(image);
    return image;
}

// Opens the image: "" when it opens, else the message it is refused with.
std::string Open(const std::vector<uint8_t> &image) {
    WriteBytes(HAND_DB_PATH, image, image.size());
    DatabaseConfig config;
    config.path = HAND_DB_PATH;
    config.read_only = true;
    try {
        Database db(config);
    } catch (const std::exception &e) {
        return e.what();
    }
    return "";
}

std::string Open(const Bytes &tables) { return Open(Image(tables)); }

bool Has(const std::string &message, const std::string &part) {
    return message.find(part) != std::string::npos;
}

} // namespace

TEST_CASE("Load - the header fields Save writes as constants") {
    auto good = Image(Bytes().U32(1).EmptyTable("t"));
    CHECK(Open(good) == "");

    auto bad = good;
    PutU32(bad, 8, BLOCK_SIZE + 1);
    Seal(bad);
    CHECK(Has(Open(bad), "the header's block size is 262145, not 262144"));

    bad = good;
    PutU64(bad, 32, 1);
    Seal(bad);
    CHECK(Has(Open(bad), "the header's block count is 1, not 0"));

    // data_offset one past 4096, and one short of it.
    for (uint64_t offset : {uint64_t(4095), uint64_t(4097)}) {
        CAPTURE(offset);
        bad = good;
        PutU64(bad, 40, offset);
        Seal(bad);
        CHECK(Has(Open(bad), "the header records the tables at offsets 4096 and " +
                                 std::to_string(offset) + ", not 4096"));
        bad = good;
        PutU64(bad, 16, offset);
        Seal(bad);
        CHECK(Has(Open(bad), "the header records the tables at offsets " +
                                 std::to_string(offset) + " and 4096, not 4096"));
    }

    // The bytes between the header and the tables, at each end.
    for (size_t at : {size_t(48), size_t(4095)}) {
        CAPTURE(at);
        bad = good;
        bad[at] = 1;
        Seal(bad);
        CHECK(Has(Open(bad), "byte at offset " + std::to_string(at) +
                                 ", between the header and the tables, is not zero"));
    }
    std::remove(HAND_DB_PATH);
}

TEST_CASE("Load - the table count is bounded by the bytes left") {
    // Two tables of 23 bytes each: the bound is 46 / 21 = 2.
    CHECK(Open(Bytes().U32(2).EmptyTable("a").EmptyTable("b")) == "");
    CHECK(Has(Open(Bytes().U32(3).EmptyTable("a").EmptyTable("b")),
              "table count 3 at offset 4096 is more than the 46 bytes left can hold"));
    // With 16 bytes more the bound is still 2; with 17 it is 63 / 21 = 3,
    // and the third table is read (and refused for what it holds).
    CHECK(Has(Open(Bytes().U32(3).EmptyTable("a").EmptyTable("b").Zeros(16)),
              "table count 3 at offset 4096 is more than the 62 bytes left can hold"));
    CHECK(Has(Open(Bytes().U32(3).EmptyTable("a").EmptyTable("b").Zeros(17)),
              "a table with no columns at offset 4150"));
    // No tables, and nothing after the count.
    CHECK(Open(Bytes().U32(0)) == "");
    CHECK(Has(Open(Bytes().U32(1)), "table count 1 at offset 4096 is more than the 0 bytes left"));
    // The count itself cut short.
    CHECK(Has(Open(Bytes().U8(0).U8(0).U8(0)),
              "the table count at offset 4096 takes 4 bytes, but only 3 are left"));
    std::remove(HAND_DB_PATH);
}

TEST_CASE("Load - a string's length is bounded by the bytes left") {
    // A table name of exactly the bytes left passes its own check, and the
    // file then ends where the column count should be.
    CHECK(Has(Open(Bytes().U32(1).U32(17).Zeros(17)),
              "a column count at offset 4121 takes 4 bytes, but only 0 are left"));
    CHECK(Has(Open(Bytes().U32(1).U32(18).Zeros(17)),
              "a table name at offset 4100 is 18 bytes long, but only 17 are left"));
    // A VARCHAR value: of exactly the rest of the file, and one byte more.
    auto value = [](uint32_t len, size_t have) {
        return Bytes().U32(1).Str("t").U32(1).Str("s").U8(uint8_t(LogicalTypeId::VARCHAR))
            .U64(1).U8(0).U32(len).Zeros(have);
    };
    CHECK(Open(value(5, 5)) == "");
    CHECK(Has(Open(value(6, 5)), "a VARCHAR or BLOB value at offset 4124 is 6 bytes long, but only 5 are left"));
    // A column name: of exactly the bytes left, then the file ends where its
    // type should be; and one byte more.
    CHECK(Has(Open(Bytes().U32(1).Str("t").U32(1).U32(13).Zeros(13)),
              "a column type at offset 4126 takes 1 bytes, but only 0 are left"));
    CHECK(Has(Open(Bytes().U32(1).Str("t").U32(1).U32(14).Zeros(13)),
              "a column name at offset 4109 is 14 bytes long, but only 13 are left"));
    std::remove(HAND_DB_PATH);
}

TEST_CASE("Load - the column count is bounded by the bytes left") {
    // After the count, two columns of 6 bytes and the row count's 8: the
    // bound is 20 / 5 = 4. At 4 the loader reads a third column from the
    // row count's zeros, an empty name and type 0.
    auto table = [](uint32_t count) {
        return Bytes().U32(1).Str("t").U32(count).Str("a").U8(5).Str("b").U8(5).U64(0);
    };
    CHECK(Open(table(2)) == "");
    CHECK(Has(Open(table(4)), "unknown column type 0 at offset 4125"));
    CHECK(Has(Open(table(5)), "column count 5 at offset 4105 is more than the 20 bytes left can hold"));
    CHECK(Has(Open(table(0)), "a table with no columns at offset 4105"));
    std::remove(HAND_DB_PATH);
}

TEST_CASE("Load - a column type outside the ones Save writes is refused") {
    // The tags of LogicalTypeId that a table made by SQL can have.
    std::vector<uint8_t> known = {1, 2, 3, 4, 5, 6, 7, 12, 13, 15, 16, 17, 18, 19, 21, 22};
    for (unsigned tag = 0; tag < 256; tag++) {
        CAPTURE(tag);
        auto message = Open(Bytes().U32(1).EmptyTable("t", uint8_t(tag)));
        bool is_known = std::find(known.begin(), known.end(), tag) != known.end();
        if (is_known) {
            CHECK(message == "");
        } else {
            CHECK(Has(message, "unknown column type " + std::to_string(tag) + " at offset 4114"));
        }
    }
    std::remove(HAND_DB_PATH);
}

TEST_CASE("Load - the row count is bounded by the bytes left") {
    // Two columns: each row takes at least two null flags. With 6 flags
    // left, 3 rows fit.
    auto table = [](uint64_t rows) {
        return Bytes().U32(1).Str("t").U32(2).Str("a").U8(5).Str("b").U8(15).U64(rows)
            .U8(1).U8(1).U8(1).U8(1).U8(1).U8(1);
    };
    CHECK(Open(table(3)) == "");
    CHECK(Has(Open(table(4)), "row count 4 at offset 4121 is more than the 6 bytes left can hold"));
    CHECK(Has(Open(table(UINT64_MAX)), "row count 18446744073709551615 at offset 4121"));
    std::remove(HAND_DB_PATH);
}

TEST_CASE("Load - each value is checked against its type") {
    auto one = [](LogicalTypeId type, const Bytes &value) {
        auto b = Bytes().U32(1).Str("t").U32(1).Str("v").U8(uint8_t(type)).U64(1);
        b.b.insert(b.b.end(), value.b.begin(), value.b.end());
        return b;
    };
    // The null flag: 0 and 1, then 2.
    CHECK(Open(one(LogicalTypeId::INTEGER, Bytes().U8(1))) == "");
    CHECK(Open(one(LogicalTypeId::INTEGER, Bytes().U8(0).I32(7))) == "");
    CHECK(Has(Open(one(LogicalTypeId::INTEGER, Bytes().U8(2).I32(7))),
              "null flag 2 at offset 4123 is neither 0 nor 1"));
    // BOOLEAN: 1, then 2.
    CHECK(Open(one(LogicalTypeId::BOOLEAN, Bytes().U8(0).U8(1))) == "");
    CHECK(Has(Open(one(LogicalTypeId::BOOLEAN, Bytes().U8(0).U8(2))),
              "BOOLEAN value 2 at offset 4124 is neither 0 nor 1"));
    // SMALLINT, which Save writes in 4 bytes: each end of its range, then
    // one past.
    CHECK(Open(one(LogicalTypeId::SMALLINT, Bytes().U8(0).I32(32767))) == "");
    CHECK(Open(one(LogicalTypeId::SMALLINT, Bytes().U8(0).I32(-32768))) == "");
    CHECK(Has(Open(one(LogicalTypeId::SMALLINT, Bytes().U8(0).I32(32768))),
              "SMALLINT value 32768 at offset 4124 is out of range"));
    CHECK(Has(Open(one(LogicalTypeId::SMALLINT, Bytes().U8(0).I32(-32769))),
              "SMALLINT value -32769 at offset 4124 is out of range"));
    // A fixed-width value cut short.
    CHECK(Has(Open(one(LogicalTypeId::BIGINT, Bytes().U8(0).Zeros(7))),
              "a BIGINT, TIMESTAMP or TIME value at offset 4124 takes 8 bytes, but only 7 are left"));
    CHECK(Has(Open(one(LogicalTypeId::DOUBLE, Bytes().U8(0).Zeros(7))),
              "a DOUBLE value at offset 4124 takes 8 bytes, but only 7 are left"));
    // Types whose values Save writes as text the engine cannot read back:
    // NULL loads, anything else is refused.
    for (auto type : {LogicalTypeId::SQLNULL, LogicalTypeId::HUGEINT, LogicalTypeId::INTERVAL,
                      LogicalTypeId::UUID}) {
        CAPTURE(int(type));
        CHECK(Open(one(type, Bytes().U8(1))) == "");
        CHECK(Has(Open(one(type, Bytes().U8(0).Str("1"))), "value at offset 4124, which this engine cannot read back"));
    }
    std::remove(HAND_DB_PATH);
}

TEST_CASE("Load - bytes after the last table, and a table name twice, are refused") {
    CHECK(Open(Bytes().U32(1).EmptyTable("t")) == "");
    CHECK(Has(Open(Bytes().U32(1).EmptyTable("t").U8(0)), "1 bytes after the last table at offset 4123"));
    CHECK(Open(Bytes().U32(2).EmptyTable("t").EmptyTable("u")) == "");
    CHECK(Has(Open(Bytes().U32(2).EmptyTable("t").EmptyTable("t")),
              "a second table of the same name at offset 4123"));
    std::remove(HAND_DB_PATH);
}

TEST_CASE("Load - a refused file adds nothing to the catalog") {
    // The first table is good, the second is refused.
    auto image = Image(Bytes().U32(2).EmptyTable("good").Str("bad").U32(0).Zeros(20));
    WriteBytes(HAND_DB_PATH, image, image.size());
    {
        Catalog catalog;
        CHECK_THROWS_WITH(Checkpoint::Load(catalog, HAND_DB_PATH),
                          doctest::Contains("a table with no columns"));
        CHECK(catalog.GetSchema().GetTableNames().empty());
    }
    // A table the catalog already has is refused, and the catalog keeps
    // what it had.
    image = Image(Bytes().U32(2).EmptyTable("new").EmptyTable("old"));
    WriteBytes(HAND_DB_PATH, image, image.size());
    {
        Catalog catalog;
        catalog.CreateTable("old", {ColumnDefinition("x", LogicalType::VARCHAR())});
        CHECK_THROWS_WITH(Checkpoint::Load(catalog, HAND_DB_PATH),
                          doctest::Contains("a second table of the same name"));
        CHECK(catalog.GetSchema().GetTableNames() == std::vector<std::string>{"old"});
        CHECK(catalog.GetTable("old")->GetColumns()[0].type.id() == LogicalTypeId::VARCHAR);
    }
    std::remove(HAND_DB_PATH);
}

TEST_CASE("Load - row groups are sized to the rows the file holds") {
    // ROW_GROUP_SIZE + 1 NULL BOOLEANs: a full group and a group of one,
    // each allocated for its own rows only.
    auto b = Bytes().U32(1).Str("t").U32(1).Str("b").U8(uint8_t(LogicalTypeId::BOOLEAN))
                 .U64(ROW_GROUP_SIZE + 1);
    b.b.insert(b.b.end(), ROW_GROUP_SIZE + 1, 1);
    auto image = Image(b);
    WriteBytes(HAND_DB_PATH, image, image.size());
    {
        Database db(HAND_DB_PATH);
        auto &storage = db.GetCatalog().GetTable("t")->GetStorage();
        REQUIRE(storage.RowGroupCount() == 2);
        CHECK(storage.GetRowGroup(0).Capacity() == ROW_GROUP_SIZE);
        CHECK(storage.GetRowGroup(1).Capacity() == 1);
        CHECK(storage.GetRowGroup(1).Count() == 1);
        // Rows inserted after the load go to a new group.
        Connection conn(db);
        conn.Query("INSERT INTO t VALUES (true), (false)");
        CHECK(conn.Query("SELECT count(*), count(b) FROM t").GetValue(0, 0).GetValue<int64_t>() ==
              int64_t(ROW_GROUP_SIZE) + 3);
        CHECK(conn.Query("SELECT count(b) FROM t").GetValue(0, 0).GetValue<int64_t>() == 2);
    }
    {
        // Saved on close and read back.
        Database db(HAND_DB_PATH);
        Connection conn(db);
        CHECK(conn.Query("SELECT count(*) FROM t").GetValue(0, 0).GetValue<int64_t>() ==
              int64_t(ROW_GROUP_SIZE) + 3);
    }
    std::remove(HAND_DB_PATH);
}

TEST_CASE("Load - found by the fuzzer: a TIME of INT64_MIN microseconds") {
    // test/fixtures/fuzz/checkpoint_load_time_int64_min.db: one table, one
    // TIME column, one row of INT64_MIN. Rendering it overflowed in
    // FormatTime (a UBSan report). The same file is built here by hand.
    auto fixture = ReadBytes(FIXTURES + "fuzz/checkpoint_load_time_int64_min.db");
    auto built = Image(Bytes().U32(1).Str("t").U32(1).Str("x").U8(uint8_t(LogicalTypeId::TIME))
                           .U64(1).U8(0).I64(INT64_MIN));
    CHECK(fixture == built);
    WriteBytes(HAND_DB_PATH, fixture, fixture.size());
    {
        DatabaseConfig config;
        config.path = HAND_DB_PATH;
        config.read_only = true;
        Database db(config);
        Connection conn(db);
        auto r = conn.Query("SELECT x FROM t");
        REQUIRE(r.RowCount() == 1);
        CHECK(r.GetValue(0, 0).ToString() == "19:59:05.224192");
    }
    std::remove(HAND_DB_PATH);
}

TEST_CASE("Load - the extreme DATE and TIMESTAMP values a file can hold render") {
    // Expected text from the same civil-from-days algorithm in Python's
    // unbounded integers. DaysToYMD overflowed past INT32_MAX - 719468
    // days, and a TIMESTAMP's day count was cut to 32 bits.
    auto b = Bytes().U32(1).Str("t").U32(2).Str("d").U8(uint8_t(LogicalTypeId::DATE))
                 .Str("ts").U8(uint8_t(LogicalTypeId::TIMESTAMP)).U64(2)
                 .U8(0).I32(INT32_MAX).U8(0).I64(INT64_MAX)
                 .U8(0).I32(INT32_MIN).U8(0).I64(INT64_MIN);
    auto image = Image(b);
    WriteBytes(HAND_DB_PATH, image, image.size());
    {
        DatabaseConfig config;
        config.path = HAND_DB_PATH;
        config.read_only = true;
        Database db(config);
        Connection conn(db);
        auto r = conn.Query("SELECT d, ts FROM t");
        REQUIRE(r.RowCount() == 2);
        CHECK(r.GetValue(0, 0).ToString() == "5881580-07-11");
        CHECK(r.GetValue(0, 1).ToString() == "294247-01-10 04:00:54.775807");
        CHECK(r.GetValue(1, 0).ToString() == "-5877641-06-23");
        CHECK(r.GetValue(1, 1).ToString() == "-290308-12-21 19:59:05.224192");
    }
    std::remove(HAND_DB_PATH);
}

// ============================================================================
// HUGEINT, INTERVAL and UUID are refused by SQL (Stage 4, design L12)
// ============================================================================

namespace {

// Runs `sql` on `conn`; returns the error message, "" when it succeeds.
std::string QueryError(Connection &conn, const std::string &sql) {
    try {
        conn.Query(sql);
    } catch (const std::exception &e) {
        return e.what();
    }
    return "";
}

void CheckTypesRefused(Database &db, Connection &conn) {
    auto tables = db.GetCatalog().GetSchema().GetTableNames();
    std::sort(tables.begin(), tables.end());
    for (std::string type : {"HUGEINT", "INTERVAL", "UUID", "hugeint", "uuid"}) {
        CAPTURE(type);
        std::string upper = type;
        for (auto &c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        auto refused = upper + " is not supported";
        CHECK(Has(QueryError(conn, "CREATE TABLE r (x " + type + ")"), refused));
        CHECK(Has(QueryError(conn, "CREATE TABLE r2 (a INTEGER, x " + type + ", b VARCHAR)"), refused));
        CHECK(Has(QueryError(conn, "ALTER TABLE keep ADD COLUMN x " + type), refused));
        CHECK(Has(QueryError(conn, "SELECT CAST(5 AS " + type + ")"), refused));
        CHECK(Has(QueryError(conn, "SELECT TRY_CAST('5' AS " + type + ")"), refused));
        CHECK(Has(QueryError(conn, "SELECT a::" + type + " FROM keep"), refused));
        CHECK(Has(QueryError(conn, "CREATE TABLE r3 AS SELECT CAST(NULL AS " + type + ") AS x"), refused));
    }
    // No table was made and none was changed.
    auto after = db.GetCatalog().GetSchema().GetTableNames();
    std::sort(after.begin(), after.end());
    CHECK(after == tables);
    CHECK(conn.Query("SELECT * FROM keep").ColumnCount() == 2);
}

} // namespace

TEST_CASE("Types - HUGEINT, INTERVAL and UUID are refused, in memory and in a file") {
    {
        Database db;
        Connection conn(db);
        conn.Query("CREATE TABLE keep (a INTEGER, s VARCHAR)");
        conn.Query("INSERT INTO keep VALUES (1, 'one'), (2, NULL)");
        CheckTypesRefused(db, conn);
    }
    std::remove(TEST_DB_PATH);
    {
        Database db(TEST_DB_PATH);
        Connection conn(db);
        conn.Query("CREATE TABLE keep (a INTEGER, s VARCHAR)");
        conn.Query("INSERT INTO keep VALUES (1, 'one'), (2, NULL)");
        conn.Query("CREATE TABLE other (d DOUBLE)");
        conn.Query("INSERT INTO other VALUES (1.5)");
        CheckTypesRefused(db, conn);
    }
    {
        // Closed, saved and reopened: the tables are all there, as they were.
        Database db(TEST_DB_PATH);
        Connection conn(db);
        auto names = db.GetCatalog().GetSchema().GetTableNames();
        std::sort(names.begin(), names.end());
        CHECK(names == std::vector<std::string>{"keep", "other"});
        auto r = conn.Query("SELECT a, s FROM keep ORDER BY a");
        REQUIRE(r.RowCount() == 2);
        CHECK(r.ColumnCount() == 2);
        CHECK(r.GetValue(0, 1).GetValue<std::string>() == "one");
        CHECK(r.GetValue(1, 1).IsNull());
        CHECK(conn.Query("SELECT d FROM other").GetValue(0, 0).GetValue<double>() == 1.5);
    }
    std::remove(TEST_DB_PATH);
}

TEST_CASE("Types - every type SQL accepts round-trips its extremes through the file") {
    // Each type name below that CREATE TABLE accepts must have values here
    // (its least, its greatest, its zero, and NULL) and give them back after
    // the database is closed, saved and opened again; a type accepted with
    // no file encoding fails this test. The names are DuckDB's, and the
    // engine's aliases.
    auto values = [](LogicalTypeId id, std::vector<Value> &out) {
        const std::string big(300, '\xff');
        switch (id) {
        case LogicalTypeId::BOOLEAN:
            out = {Value::BOOLEAN(false), Value::BOOLEAN(true), Value::BOOLEAN(false)};
            return true;
        case LogicalTypeId::TINYINT:
            out = {Value::TINYINT(INT8_MIN), Value::TINYINT(INT8_MAX), Value::TINYINT(0)};
            return true;
        case LogicalTypeId::SMALLINT:
            out = {Value::SMALLINT(INT16_MIN), Value::SMALLINT(INT16_MAX), Value::SMALLINT(0)};
            return true;
        case LogicalTypeId::INTEGER:
            out = {Value::INTEGER(INT32_MIN), Value::INTEGER(INT32_MAX), Value::INTEGER(0)};
            return true;
        case LogicalTypeId::BIGINT:
            out = {Value::BIGINT(INT64_MIN), Value::BIGINT(INT64_MAX), Value::BIGINT(0)};
            return true;
        case LogicalTypeId::FLOAT:
            out = {Value::FLOAT(-3.4028235e38f), Value::FLOAT(3.4028235e38f), Value::FLOAT(0.0f)};
            return true;
        case LogicalTypeId::DOUBLE:
            out = {Value::DOUBLE(-1.7976931348623157e308), Value::DOUBLE(1.7976931348623157e308),
                   Value::DOUBLE(0.0)};
            return true;
        case LogicalTypeId::VARCHAR:
            out = {Value::VARCHAR(""), Value::VARCHAR(big), Value::VARCHAR("0")};
            return true;
        case LogicalTypeId::BLOB:
            out = {Value::BLOB(""), Value::BLOB(big), Value::BLOB(std::string(1, '\0'))};
            return true;
        case LogicalTypeId::DATE:
            out = {Value::DATE(INT32_MIN), Value::DATE(INT32_MAX), Value::DATE(0)};
            return true;
        case LogicalTypeId::TIME:
            out = {Value::TIME(INT64_MIN), Value::TIME(INT64_MAX), Value::TIME(0)};
            return true;
        case LogicalTypeId::TIMESTAMP:
            out = {Value::TIMESTAMP(INT64_MIN), Value::TIMESTAMP(INT64_MAX), Value::TIMESTAMP(0)};
            return true;
        default:
            return false;
        }
    };
    auto text = [](const QueryResult &r) {
        std::string out;
        for (auto &t : r.column_types) out += t.ToString() + ";";
        for (idx_t i = 0; i < r.RowCount(); i++) {
            for (idx_t c = 0; c < r.ColumnCount(); c++) {
                auto v = r.GetValue(i, c);
                out += v.IsNull() ? std::string("NULL|") : std::to_string(v.ToString().size()) + ":" + v.ToString() + "|";
            }
            out += "\n";
        }
        return out;
    };

    const char *names[] = {
        "BOOLEAN", "BOOL", "TINYINT", "INT1", "SMALLINT", "INT2", "INTEGER", "INT", "INT4",
        "BIGINT", "INT8", "HUGEINT", "UHUGEINT", "UTINYINT", "USMALLINT", "UINTEGER", "UBIGINT",
        "FLOAT", "REAL", "FLOAT4", "DOUBLE", "FLOAT8", "DECIMAL", "DECIMAL(18,3)", "NUMERIC",
        "VARCHAR", "VARCHAR(5)", "TEXT", "STRING", "CHAR", "BLOB", "BYTEA", "BIT", "DATE",
        "TIME", "TIMETZ", "TIMESTAMP", "TIMESTAMPTZ", "TIMESTAMP_S", "TIMESTAMP_MS",
        "TIMESTAMP_NS", "DATETIME", "INTERVAL", "UUID", "JSON", "ENUM", "LIST", "MAP",
        "STRUCT", "UNION", "INTEGER[]", "VARINT", "NULL"};
    idx_t accepted = 0;
    for (std::string name : names) {
        CAPTURE(name);
        std::remove(TEST_DB_PATH);
        std::string before;
        {
            Database db(TEST_DB_PATH);
            Connection conn(db);
            if (!QueryError(conn, "CREATE TABLE t (v " + name + ")").empty()) continue;
            accepted++;
            auto *entry = db.GetCatalog().GetTable("t");
            REQUIRE(entry != nullptr);
            auto types = entry->GetTypes();
            std::vector<Value> vals;
            INFO("type ", types[0].ToString(), " is accepted by SQL but has no round-trip values");
            REQUIRE(values(types[0].id(), vals));
            vals.push_back(Value());
            DataChunk chunk;
            chunk.Initialize(types);
            for (idx_t i = 0; i < vals.size(); i++) chunk.SetValue(0, i, vals[i]);
            entry->GetStorage().Append(chunk);
            before = text(conn.Query("SELECT * FROM t"));
            CHECK(conn.Query("SELECT count(*) FROM t").GetValue(0, 0).GetValue<int64_t>() == 4);
        }
        {
            Database db(TEST_DB_PATH);
            Connection conn(db);
            CHECK(text(conn.Query("SELECT * FROM t")) == before);
        }
    }
    CHECK(accepted >= 20);
    std::remove(TEST_DB_PATH);
}
