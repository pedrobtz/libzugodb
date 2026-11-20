#!/bin/sh
# Check that docs/dialect.md names the functions the binder knows, and only
# those (roadmap Stage 9, design L26). The binder's list is every quoted
# name between the BEGIN and END FUNCTION REGISTRY markers of
# src/binder/binder.cpp; the doc's is every `name` in backticks, a
# lowercase identifier, between its BEGIN and END FUNCTIONS markers. Prints
# each name on one side only and exits 1 if there is any.
#
#   tools/check_dialect.sh
#
# Run from the repository root; the ci jobs run it.

set -eu

binder=src/binder/binder.cpp
doc=docs/dialect.md
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

sed -n '/BEGIN FUNCTION REGISTRY/,/END FUNCTION REGISTRY/p' "$binder" |
    grep -v '^ *//' | grep -oE '"[A-Z][A-Z0-9_]*"' | tr -d '"' |
    tr '[:upper:]' '[:lower:]' | sort -u > "$tmp/binder"
sed -n '/BEGIN FUNCTIONS/,/END FUNCTIONS/p' "$doc" |
    grep -oE '`[a-z][a-z0-9_]*`' | tr -d '`' | sort -u > "$tmp/doc"

if [ ! -s "$tmp/binder" ] || [ ! -s "$tmp/doc" ]; then
    echo "check_dialect: no function list found in $binder or $doc (markers moved?)" >&2
    exit 1
fi

status=0
only_binder=$(comm -23 "$tmp/binder" "$tmp/doc")
only_doc=$(comm -13 "$tmp/binder" "$tmp/doc")
if [ -n "$only_binder" ]; then
    echo "In the binder's registry but not in $doc:" >&2
    printf '  %s\n' $only_binder >&2
    status=1
fi
if [ -n "$only_doc" ]; then
    echo "In $doc but not in the binder's registry:" >&2
    printf '  %s\n' $only_doc >&2
    status=1
fi
if [ "$status" = 0 ]; then
    echo "check_dialect: $(wc -l < "$tmp/binder" | tr -d ' ') functions, the same in $binder and $doc"
fi
exit $status
