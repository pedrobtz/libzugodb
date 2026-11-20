#!/bin/sh
# Write the database-file fixtures of test/unit/storage/test_persistence.cpp
# from tools/db_fixture.sql, with the engine's own shell.
#
#   tools/make_db_fixtures.sh          # test/fixtures/golden_v1001.db
#   tools/make_db_fixtures.sh v1       # test/fixtures/format_v1.db
#
# golden_v1001.db is written by the shell built from this tree, so rerun it
# only with a change that bumps the format version (and rename the file to
# the new version): the point of the fixture is that a layout change made
# without the bump fails its test. format_v1.db is written by the shell
# built at 030f7b8, the last commit with upstream SlothDB's format
# (version 1, no checksum), in a temporary git worktree.
#
# Run from the repository root. The test checks the rows the SQL inserts,
# not the bytes, so the file may differ by platform.

set -eu

SQL=tools/db_fixture.sql

write() { # shell binary, output file
    rm -f "$2" "$2.tmp"
    "$1" "$2" < "$SQL" > /dev/null
    echo "$2: $(wc -c < "$2" | tr -d ' ') bytes"
}

case "${1:-}" in
"")
    cmake -S . -B build-fixtures -DCMAKE_BUILD_TYPE=Release -DSLOTHDB_BUILD_SHELL=ON > /dev/null
    cmake --build build-fixtures --target slothdb -j4 > /dev/null 2>&1
    write build-fixtures/src/slothdb test/fixtures/golden_v1001.db
    ;;
v1)
    tree=$(mktemp -d)
    git worktree add --detach "$tree" 030f7b8 > /dev/null 2>&1
    cmake -S "$tree" -B "$tree/build" -DCMAKE_BUILD_TYPE=Release -DSLOTHDB_BUILD_SHELL=ON > /dev/null
    cmake --build "$tree/build" --target slothdb -j4 > /dev/null 2>&1
    write "$tree/build/src/slothdb" test/fixtures/format_v1.db
    git worktree remove --force "$tree"
    ;;
*)
    echo "usage: $0 [v1]" >&2
    exit 1
    ;;
esac
