#!/bin/sh
# Runs the pinned subset with the given zugodb_slt at thread caps 1 and 2,
# and compares the counts, the failures and the thread differences with
# the committed ones (exit 1 if any differs, in either direction). With
# --update, writes them into test/conformance/ instead.
#
#   test/conformance/run.sh <zugodb_slt> [--update] [corpus dir]
#
# The committed outputs are made by the build the `conformance` CI job
# uses: RelWithDebInfo with -DSLOTHDB_SANITIZERS=ON (ASan and UBSan), so
# that a memory error is a crash every time and not by chance (design L16).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
slt=$1
shift
update=0
if [ "${1:-}" = "--update" ]; then
    update=1
    shift
fi
corpus=${1:-$here/corpus}
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
"$slt" run --corpus "$corpus" --subset "$here/subset.txt" --out "$out" --caps 1,2 --jobs "$jobs"
if [ -n "${CONFORMANCE_OUT:-}" ]; then
    mkdir -p "$CONFORMANCE_OUT"
    cp "$out"/*.tsv "$CONFORMANCE_OUT"/
fi
status=0
for f in counts.tsv totals.tsv failures.tsv threads.tsv; do
    if [ "$update" = 1 ]; then
        cp "$out/$f" "$here/$f"
    elif ! diff -u "$here/$f" "$out/$f"; then
        status=1
    fi
done
if [ "$status" != 0 ]; then
    echo "The conformance counts differ from the committed ones (above: - committed, + this run)." >&2
    echo "If the change is meant, run test/conformance/run.sh <zugodb_slt> --update and commit." >&2
fi
exit $status
