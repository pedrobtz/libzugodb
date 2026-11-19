"""Write the Parquet fixtures of test/unit/execution/test_worker_errors.cpp.

test/fixtures/groups.parquet is valid: k BIGINT (i % 37), s VARCHAR
('s' || i % 11) and v BIGINT (i), for i in 0 to 16383, Snappy, eight row
groups of 2048 rows, so a scan of it starts worker threads.

test/fixtures/damaged_zstd.parquet is the damaged Parquet file of
SouravRoy-ETL/slothdb#33 in small. The file is valid Parquet written by DuckDB (one BIGINT column `x`, 0 to
19999, ZSTD, ten row groups of 2048 rows but the last), then damaged: the
first byte of every zstd frame's magic number (28 B5 2F FD) before the
footer is inverted. The footer and the page headers are left intact, so the
file opens and the error comes from decompressing a page, which the engine
does on a decode worker thread.

    python3 -m venv /tmp/venv && /tmp/venv/bin/pip install duckdb==1.1.3
    /tmp/venv/bin/python tools/make_parquet_fixtures.py

Run from the repository root. The output is committed; rerunning the script
gives the same bytes for the same DuckDB version.
"""

import os
import sys
import tempfile

import duckdb

OUT = os.path.join("test", "fixtures", "damaged_zstd.parquet")
GROUPS = os.path.join("test", "fixtures", "groups.parquet")
MAGIC = b"\x28\xb5\x2f\xfd"


def main() -> int:
    duckdb.connect().sql(
        "COPY (SELECT (i % 37)::BIGINT AS k, ('s' || (i % 11))::VARCHAR AS s, "
        "i::BIGINT AS v FROM range(16384) t(i)) "
        f"TO '{GROUPS}' (FORMAT PARQUET, COMPRESSION SNAPPY, ROW_GROUP_SIZE 2048)"
    )
    print(f"{GROUPS}: {os.path.getsize(GROUPS)} bytes")

    with tempfile.TemporaryDirectory() as tmp:
        valid = os.path.join(tmp, "valid.parquet")
        duckdb.connect().sql(
            "COPY (SELECT i::BIGINT AS x FROM range(20000) t(i)) "
            f"TO '{valid}' (FORMAT PARQUET, COMPRESSION ZSTD, ROW_GROUP_SIZE 2048)"
        )
        data = bytearray(open(valid, "rb").read())

    footer_len = int.from_bytes(data[-8:-4], "little")
    footer_start = len(data) - 8 - footer_len
    frames = 0
    i = data.find(MAGIC, 4)
    while 0 <= i < footer_start:
        data[i] ^= 0xFF
        frames += 1
        i = data.find(MAGIC, i + len(MAGIC))
    if frames == 0:
        print("no zstd frame found", file=sys.stderr)
        return 1

    with open(OUT, "wb") as f:
        f.write(data)
    print(f"{OUT}: {len(data)} bytes, {frames} zstd frames damaged")
    return 0


if __name__ == "__main__":
    sys.exit(main())
