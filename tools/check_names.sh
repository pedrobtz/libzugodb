#!/bin/sh
# Nothing is named after SlothDB but the credit (roadmap Stage 12, #17,
# design L31): no `slothdb` or `SLOTHDB` in a tracked file or path, case
# sensitive, so that the provenance prose "SlothDB" stays. Excluded:
#
# - LICENSE and docs/SLOTHDB-README.md, the credit;
# - CHANGELOG.md, whose entries are history;
# - .agents/, the design and roadmap, whose past stages are history;
# - tools/make_db_fixtures.sh, which builds commits before the rename
#   under their names;
# - this file.
#
# Allowed anywhere: the upstream repository, `SouravRoy-ETL/slothdb`, in
# a link or an issue reference, and the credit's file name.
#
#   tools/check_names.sh
#
# Run from the repository root; the ci jobs run it.

set -eu

pattern='slothdb|SLOTHDB'
allowed='SouravRoy-ETL/slothdb|SLOTHDB-README\.md'

found=$(git grep -n -I -E "$pattern" -- . \
        ':!LICENSE' ':!docs/SLOTHDB-README.md' ':!CHANGELOG.md' ':!.agents' \
        ':!tools/make_db_fixtures.sh' ':!tools/check_names.sh' |
    sed -E "s#$allowed##g" | grep -E "$pattern" || true)
paths=$(git ls-files | grep -v '^docs/SLOTHDB-README\.md$' | grep -E "$pattern" || true)

status=0
if [ -n "$found" ]; then
    echo "check_names: SlothDB's name is left in:" >&2
    echo "$found" >&2
    status=1
fi
if [ -n "$paths" ]; then
    echo "check_names: a path carries SlothDB's name:" >&2
    echo "$paths" >&2
    status=1
fi
[ "$status" -eq 0 ] && echo "check_names: no slothdb or SLOTHDB outside the credit and the history"
exit "$status"
