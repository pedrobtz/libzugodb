#!/bin/sh
# No symbol of a library removed in Stage 11 (#16) is left in the engine:
# zstd, snappy, fastpforlib and miniz went with the file readers. Run from
# the repository root, after a build:
#
#   tools/check_symbols.sh build/src/libslothdb.a
#
# nm's output is not demangled: a C++ namespace (snappy, FastPForLib) is in
# every mangled name it holds, defined or undefined. The vendored paths
# are checked too, since zugodb builds from them and not from CMake.

set -eu

lib=${1:?usage: $0 <path to libslothdb.a>}

status=0

for path in third_party/zstd third_party/snappy third_party/fastpforlib \
            src/storage/miniz.c src/storage/miniz.h; do
    if [ -e "$path" ]; then
        echo "check_symbols: $path is back" >&2
        status=1
    fi
done

symbols=$(mktemp)
trap 'rm -f "$symbols"' EXIT
if ! nm "$lib" > "$symbols"; then
    echo "check_symbols: nm failed on $lib" >&2
    exit 1
fi
# No vacuous pass: the engine's own symbols are there.
if ! grep -q 'zugodb' "$symbols"; then
    echo "check_symbols: no engine symbol in $lib" >&2
    exit 1
fi

pattern='ZSTD_|ZSTDv|HUF_|FSE_|snappy|mz_|tinfl_|tdefl_|FastPForLib|fastunpack|fastpack'
if grep -E "$pattern" "$symbols"; then
    echo "check_symbols: a removed library's symbols are in $lib (above)" >&2
    status=1
fi

[ "$status" -eq 0 ] && echo "check_symbols: no symbol of zstd, snappy, fastpforlib or miniz"
exit "$status"
