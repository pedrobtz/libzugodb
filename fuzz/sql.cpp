// libFuzzer harness for SQL text (roadmap Stage 9, design §4 and §5).
//
// Each input is SQL text, run against a small fixed schema in a new
// in-memory database: two tables of a few rows with every type a table
// made by SQL can hold and a NULL in each column, a view over them, and
// t3, of 2049 rows, one past a chunk, with a NULL each side of the
// boundary (Stage 10).
// The input runs once with the engine's thread cap at 1 and once, in a
// second database built the same way, at 2.
//
// The oracle, beyond no crash, hang or sanitizer report: the input gets
// the same class at both caps, a result or an error, and a result has the
// same number of rows and columns. Values are not compared (design L16:
// the conformance subset compares classes too). A broken oracle fails
// (__builtin_trap). Inputs past 4096 bytes are skipped.
//
// The seeds, fuzz/corpus/sql/, are statements of the conformance subset
// (fuzz/make_sql_corpus.sh). Reach is the edges of src/parser/parser.cpp
// the kept corpus covers (fuzz/run.sh), against a floor in the workflow.
//
// Build: cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
//        -DSLOTHDB_FUZZ=ON -DSLOTHDB_BUILD_TESTS=OFF && cmake --build build-fuzz
// Run:   build-fuzz/fuzz/sql_fuzzer fuzz/corpus/sql

#include "slothdb/common/parallel.hpp"
#include "slothdb/common/types/data_chunk.hpp"
#include "slothdb/main/connection.hpp"
#include "slothdb/main/database.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

using namespace zugodb;

namespace {

void Check(bool ok) {
    if (!ok) __builtin_trap();
}

const char *const kSchema[] = {
    "CREATE TABLE t1(i INTEGER, b BIGINT, ti TINYINT, si SMALLINT, d DOUBLE, f FLOAT, "
    "s VARCHAR, bo BOOLEAN, dt DATE, tm TIME, ts TIMESTAMP, bl BLOB)",
    "INSERT INTO t1 VALUES "
    "(1, 10, 1, 100, 1.5, 0.5, 'a', true, DATE '2024-01-01', TIME '01:02:03', "
    "TIMESTAMP '2024-01-01 01:02:03', 'x'), "
    "(2, -20, -2, -200, -2.25, 2.0, 'bb', false, DATE '1999-12-31', TIME '23:59:59', "
    "TIMESTAMP '1999-12-31 23:59:59', ''), "
    "(3, 9223372036854775807, 127, 32767, 0.0, -1.0, 'héllo', true, DATE '2000-02-29', "
    "TIME '12:00:00', TIMESTAMP '2000-02-29 12:00:00', 'yz'), "
    "(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), "
    "(2, 0, 0, 0, 1e300, 3.5, '', NULL, DATE '1970-01-01', TIME '00:00:00', "
    "TIMESTAMP '1970-01-01 00:00:00', NULL), "
    // NaN of both signs, the infinities and -0.0 (#42).
    "(5, 1, 1, 1, 'nan'::DOUBLE, '-inf'::FLOAT, 'n', false, DATE '2024-01-01', TIME '01:02:03', "
    "TIMESTAMP '2024-01-01 01:02:03', 'x'), "
    "(6, 2, 2, 2, -('nan'::DOUBLE), 'nan'::FLOAT, 'nn', true, DATE '2024-01-02', TIME '01:02:04', "
    "TIMESTAMP '2024-01-02 01:02:03', 'y'), "
    "(7, 3, 3, 3, '-0.0'::DOUBLE, '-0.0'::FLOAT, 'z', NULL, NULL, NULL, NULL, NULL), "
    "(8, 4, 4, 4, '-inf'::DOUBLE, 'inf'::FLOAT, NULL, NULL, NULL, NULL, NULL, NULL)",
    "CREATE TABLE t2(i INTEGER, s VARCHAR, d DOUBLE)",
    "INSERT INTO t2 VALUES (1, 'a', 1.0), (2, 'b', NULL), (NULL, 'c', 3.0), (4, NULL, -4.0), "
    "(5, 'n', 'nan'::DOUBLE), (6, 'z', '-0.0'::DOUBLE), (7, 'i', 'inf'::DOUBLE)",
    "CREATE VIEW v AS SELECT t1.i, t1.s, t2.d FROM t1 JOIN t2 ON t1.i = t2.i",
};

// One statement's outcome: an error, or a result's shape.
struct Outcome {
    bool ok = false;
    uint64_t rows = 0;
    uint64_t columns = 0;
    bool operator==(const Outcome &o) const {
        return ok == o.ok && (!ok || (rows == o.rows && columns == o.columns));
    }
};

// The input's outcome in a new database at thread cap `cap`. The text
// runs as one batch, as a caller's would: Connection::Query returns the
// last statement's result, or the first error.
Outcome RunAt(unsigned cap, const std::string &sql) {
    SetThreadCap(cap);
    Database db;
    Connection conn(db);
    for (const char *setup : kSchema) conn.Query(setup);
    // t3(i BIGINT, k INTEGER, s VARCHAR): i = 0 .. 2048, k = i % 7, s = 's'
    // || i % 100; rows 2047 and 2048 NULL but for i. Through the C++ API,
    // so that each input does not parse 2049 rows of VALUES.
    conn.Query("CREATE TABLE t3(i BIGINT, k INTEGER, s VARCHAR)");
    for (int64_t base = 0; base < 2049; base += 2048) {
        int64_t n = base == 0 ? 2048 : 1;
        DataChunk chunk;
        chunk.Initialize({LogicalType::BIGINT(), LogicalType::INTEGER(), LogicalType::VARCHAR()});
        for (int64_t j = 0; j < n; j++) {
            int64_t v = base + j;
            bool null = v == 2047 || v == 2048;
            chunk.SetValue(0, (idx_t)j, Value::BIGINT(v));
            chunk.SetValue(1, (idx_t)j, null ? Value() : Value::INTEGER((int32_t)(v % 7)));
            chunk.SetValue(2, (idx_t)j, null ? Value() : Value::VARCHAR("s" + std::to_string(v % 100)));
        }
        chunk.SetCardinality((idx_t)n);
        conn.Append("t3", chunk);
    }
    Outcome o;
    try {
        auto r = conn.Query(sql);
        o.ok = true;
        o.rows = r.RowCount();
        o.columns = r.ColumnCount();
    } catch (const std::exception &) {
        o.ok = false;
    }
    return o;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 4096) return 0;
    std::string sql(reinterpret_cast<const char *>(data), size);
    Outcome one = RunAt(1, sql);
    Outcome two = RunAt(2, sql);
    Check(one == two);
    SetThreadCap(1);
    return 0;
}
