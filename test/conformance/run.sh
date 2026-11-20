#!/bin/sh
# Runs the pinned subset with the given zugodb_slt at thread caps 1 and 2,
# and compares the counts, the failures and the thread differences with
# the ones committed for this platform, test/conformance/<platform>/ (exit
# 1 if any differs, in either direction). With --update, writes them there
# instead. The platform is linux-x86_64 or macos-arm64, the conformance
# job's two; CONFORMANCE_PLATFORM overrides it. They differ where the
# engine does (undefined behaviour, the C library's strftime), not the
# runner; design L16.
#
#   test/conformance/run.sh <zugodb_slt> [--update] [corpus dir]
#
# The committed outputs are made by the build the `conformance` CI job
# uses: RelWithDebInfo with -DZUGODB_SANITIZERS=ON (ASan and UBSan), so
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
case "$(uname -s)" in
    Linux) os=linux ;;
    Darwin) os=macos ;;
    *) os=$(uname -s) ;;
esac
platform=${CONFORMANCE_PLATFORM:-$os-$(uname -m)}
dir=$here/$platform
if [ "$update" = 1 ]; then
    mkdir -p "$dir"
elif [ ! -d "$dir" ]; then
    echo "No counts committed for $platform (test/conformance/$platform/)." >&2
    exit 1
fi
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
        cp "$out/$f" "$dir/$f"
    elif ! diff -u "$dir/$f" "$out/$f"; then
        status=1
    fi
done
if [ "$status" != 0 ]; then
    echo "The conformance counts differ from those committed for $platform (above: - committed, + this run)." >&2
    echo "If the change is meant, run test/conformance/run.sh <zugodb_slt> --update and commit." >&2
fi
exit $status
