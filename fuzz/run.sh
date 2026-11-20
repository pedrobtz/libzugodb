#!/bin/sh
# Run one libFuzzer target for a fixed time, then check its reach.
#
#   fuzz/run.sh <target> <seconds> <source> <floor>
#   fuzz/run.sh checkpoint_load 120 src/storage/checkpoint.cpp 400
#
# Runs build-fuzz/fuzz/<target>_fuzzer (cmake -DSLOTHDB_FUZZ=ON) over the
# seeds in fuzz/corpus/<target>/, keeping what it finds in
# fuzz-work/<target>/ and any crash, hang or out-of-memory input in
# fuzz-artifacts/<target>/. Any of those fails the run.
#
# Reach: the inputs kept are replayed with -runs=0 -print_coverage=1, and
# the edges libFuzzer reports as covered in the functions of <source> are
# summed. The run fails when the sum is below <floor>. The floor is set
# from observed runs (see the fuzz job in .github/workflows/ci.yml).
#
# Run from the repository root.

set -eu

if [ $# -ne 4 ]; then
    echo "usage: $0 <target> <seconds> <source> <floor>" >&2
    exit 2
fi
target=$1
seconds=$2
source=$3
floor=$4

bin=${FUZZ_BUILD:-build-fuzz}/fuzz/${target}_fuzzer
seeds=fuzz/corpus/$target
work=fuzz-work/$target
artifacts=fuzz-artifacts/$target
mkdir -p "$work" "$artifacts"

"$bin" -max_total_time="$seconds" -max_len=16384 -timeout=25 -rss_limit_mb=2048 \
    -print_final_stats=1 -artifact_prefix="$artifacts/" "$work" "$seeds"

"$bin" -runs=0 -print_coverage=1 "$work" "$seeds" > "$work.coverage" 2>&1

# Lines like "COVERED_FUNC: hits: 5 edges: 3/4 <function> <file>:<line>";
# the function's name may hold spaces, the file never does.
reach=$(awk -v src="$source" '
    /^(UN)?COVERED_FUNC:/ {
        loc = $NF
        sub(/:[0-9]+$/, "", loc)
        if (length(loc) < length(src) || substr(loc, length(loc) - length(src) + 1) != src) next
        for (i = 1; i < NF; i++) if ($i == "edges:") split($(i + 1), e, "/")
        covered += e[1]; total += e[2]
    }
    END { printf "%d %d\n", covered, total }
' "$work.coverage")
covered=${reach% *}
total=${reach#* }

echo "reach: $covered of $total edges in $source (floor $floor)"
if [ "$total" -eq 0 ]; then
    echo "no coverage reported for $source: is llvm-symbolizer on PATH?" >&2
    exit 1
fi
if [ "$covered" -lt "$floor" ]; then
    echo "reach $covered is below the floor $floor" >&2
    exit 1
fi
