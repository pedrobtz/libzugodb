#!/bin/sh
# Write the seed corpus of the sql fuzzer, fuzz/corpus/sql/, from the
# statements of the conformance subset (test/conformance/subset.txt) in
# the fetched corpus (test/conformance/fetch.sh): the SQL of every
# `statement` and `query` record, one file each, deduplicated, at most
# 4096 bytes (the harness's limit). A file whose SQL holds a loop
# variable (`${x}`) is kept as it is: the text still exercises the parser.
# Of those, every one whose SHA-1 sorts among the first $count is kept
# (default 2000), so a rerun at the same tag writes the same seeds.
#
#   fuzz/make_sql_corpus.sh [count]
#
# The seeds are excerpts of DuckDB's test files, MIT-licensed as DuckDB is.
# Rerun it when the pinned tag or the subset changes. Run from the
# repository root.

set -eu

count=${1:-2000}
corpus=test/conformance/corpus
out=fuzz/corpus/sql
if [ ! -d "$corpus/test/sql" ]; then
    echo "No corpus: run test/conformance/fetch.sh first." >&2
    exit 1
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

grep -v '^#' test/conformance/subset.txt | grep . | while read -r f; do
    [ -f "$corpus/$f" ] && printf '%s\n' "$corpus/$f"
done > "$tmp/files"

# One record per file under $tmp/all/: the lines after a statement or
# query header up to an empty line or the ---- before expected results.
mkdir "$tmp/all"
xargs awk -v dir="$tmp/all" '
    function flush() {
        if (sql != "") { n++; f = dir "/" n; printf "%s", sql > f; close(f) }
        sql = ""; inrec = 0
    }
    FNR == 1 { flush() }
    /^(statement|query)( |$)/ { flush(); inrec = 1; next }
    inrec && (/^----$/ || /^$/) { flush(); next }
    inrec { sql = sql $0 "\n" }
    END { flush() }
' < "$tmp/files"

mkdir -p "$out"
rm -f "$out"/*.sql
# Deduplicate by content: one file per SHA-1, the records past 4096 bytes
# left out; then the first $count SHA-1s in sorted order.
find "$tmp/all" -type f -size -4097c -print0 | xargs -0 shasum | sort | awk '!seen[$1]++' |
    head -n "$count" | while read -r h f; do
    cp "$f" "$out/$h.sql"
done
echo "$(ls "$out" | wc -l | tr -d ' ') seeds in $out"
