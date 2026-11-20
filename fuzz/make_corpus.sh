#!/bin/sh
# Write the seed corpus of the checkpoint_load fuzzer,
# fuzz/corpus/checkpoint_load/, with the engine's own shell: each seed is a
# database file Save wrote from the SQL below, so the fuzzer starts from
# files the loader accepts. Every seed is under 16 KiB, the -max_len of
# fuzz/run.sh.
#
#   fuzz/make_corpus.sh
#
# Rerun it after a change to the file format. Run from the repository root.

set -eu

out=fuzz/corpus/checkpoint_load
cmake -S . -B build-fixtures -DCMAKE_BUILD_TYPE=Release -DZUGODB_BUILD_SHELL=ON > /dev/null
cmake --build build-fixtures --target zugodb_shell -j4 > /dev/null 2>&1
mkdir -p "$out"
rm -f "$out"/*.db

seed() { # name, then the SQL on stdin
    rm -f "$out/$1.db" "$out/$1.db.tmp"
    build-fixtures/src/zugodb "$out/$1.db" > /dev/null
    echo "$out/$1.db: $(wc -c < "$out/$1.db" | tr -d ' ') bytes"
}

seed no_tables <<'EOF'
SELECT 1;
EOF

seed every_type <<'EOF'
CREATE TABLE t (b BOOLEAN, i8 TINYINT, i16 SMALLINT, i32 INTEGER, i64 BIGINT, f FLOAT, d DOUBLE, s VARCHAR, bl BLOB, dt DATE, tm TIME, ts TIMESTAMP);
INSERT INTO t VALUES (true, -128, -32768, -2147483648, -9223372036854775807, 1.5, -0.25, 'zugodb', 'ab', DATE '2024-02-29', CAST('12:34:56' AS TIME), TIMESTAMP '2024-02-29 12:34:56.789');
INSERT INTO t VALUES (false, 127, 32767, 2147483647, 9223372036854775807, -3.0, 1e300, 'héllo', '', DATE '1969-12-31', NULL, TIMESTAMP '1969-12-31 23:59:59');
INSERT INTO t VALUES (NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
EOF

seed null_column <<'EOF'
CREATE TABLE z AS SELECT NULL AS a, 2 AS b;
EOF

# Columns of HUGEINT, INTERVAL and UUID, which SQL refuses since Stage 4 but
# a file written before may hold: only their NULLs load (design L12). SQL
# cannot make this seed, so it is written byte by byte as Save would: one
# table n (h HUGEINT, i INTERVAL, u UUID, x INTEGER) holding the row
# (NULL, NULL, NULL, 1).
python3 - "$out/null_only_types.db" <<'PY'
import struct, sys, zlib
def s(b): return struct.pack('<I', len(b)) + b
cols = [(b'h', 7), (b'i', 21), (b'u', 22), (b'x', 5)]
data = struct.pack('<I', 1) + s(b'n') + struct.pack('<I', len(cols))
data += b''.join(s(n) + bytes([t]) for n, t in cols)
data += struct.pack('<Q', 1) + bytes([1, 1, 1, 0]) + struct.pack('<i', 1)
img = bytearray(4096) + data
img[0:48] = struct.pack('<IIIIQQQQ', 0x534C4442, 1001, 262144, 0, 4096, len(data), 0, 4096)
img[12:16] = struct.pack('<I', zlib.crc32(bytes(img)))
open(sys.argv[1], 'wb').write(img)
PY
echo "$out/null_only_types.db: $(wc -c < "$out/null_only_types.db" | tr -d ' ') bytes"

seed several_tables <<'EOF'
CREATE TABLE a (x INTEGER);
INSERT INTO a VALUES (1), (2), (3);
CREATE TABLE "" (y VARCHAR);
INSERT INTO "" VALUES ('empty name');
CREATE TABLE dup (c INTEGER, c VARCHAR);
INSERT INTO dup VALUES (1, 'one');
CREATE TABLE "Mixed Case" (z DOUBLE);
CREATE TABLE empty_table (p INTEGER, q VARCHAR);
EOF

# More rows than one vector (2048), so a load reads several chunks.
{
    echo "CREATE TABLE r (b BOOLEAN);"
    printf "INSERT INTO r VALUES "
    seq 1 2100 | awk '{ printf "%s(%s)", (NR > 1 ? ", " : ""), ($1 % 3 ? "true" : ($1 % 2 ? "false" : "NULL")) }'
    echo ";"
} | seed many_rows
