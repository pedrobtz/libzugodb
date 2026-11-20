#!/bin/sh
# Fetches DuckDB's test/sql at the pinned tag (test/conformance/duckdb-tag:
# the tag and its commit) into test/conformance/corpus/, or the directory
# given, by a shallow sparse checkout, and checks the commit. The corpus is
# not committed (.gitignore).
#
#   test/conformance/fetch.sh [dir]
set -eu
here=$(cd "$(dirname "$0")" && pwd)
read -r tag commit < "$here/duckdb-tag"
dest=${1:-$here/corpus}
if [ -d "$dest/.git" ] && [ "$(git -C "$dest" rev-parse HEAD 2>/dev/null)" = "$commit" ]; then
    echo "DuckDB $tag already at $dest"
    exit 0
fi
rm -rf "$dest"
mkdir -p "$dest"
git -C "$dest" init -q
git -C "$dest" remote add origin https://github.com/duckdb/duckdb.git
git -C "$dest" sparse-checkout set --no-cone /test/sql/
git -C "$dest" fetch -q --depth 1 --filter=blob:none origin "refs/tags/$tag"
git -C "$dest" -c advice.detachedHead=false checkout -q FETCH_HEAD
got=$(git -C "$dest" rev-parse HEAD)
if [ "$got" != "$commit" ]; then
    echo "DuckDB tag $tag is commit $got, not $commit" >&2
    exit 1
fi
echo "DuckDB $tag ($commit) at $dest"
