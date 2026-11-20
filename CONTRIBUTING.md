# Contributing to libzugodb

libzugodb is the SQL engine of the R package
[zugodb](https://github.com/pedrobtz/zugodb). Its design is
[.agents/design.md](.agents/design.md) and its plan
[.agents/roadmap.md](.agents/roadmap.md); read both before changing what the
engine accepts or refuses.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
build/test/slothdb_tests                    # the whole doctest suite
build/test/slothdb_tests -tc="*Window*"     # test cases by name
```

`-DSLOTHDB_SANITIZERS=ON` builds with ASan and UBSan, and
`-DSLOTHDB_BUILD_SHELL=ON` adds the `slothdb` shell from `tools/shell/`.
CI runs the suite on Linux gcc, Linux with ASan and UBSan, macOS clang and
MinGW-w64 UCRT64; a change must pass all four.

## Code

- C++20, in the namespace `zugodb::`.
- Sources are under `src/<step>/`, their headers under
  `include/slothdb/<step>/`, included as `"slothdb/<step>/<file>.hpp"`.
  Sources are globbed: after adding a file, re-run the configure step.
- The engine refuses what it cannot answer right, is offline, writes
  nothing to `stdout` or `stderr`, and keeps every thread pool under the
  process-wide cap. `CLAUDE.md` lists these invariants.

## Adding a scalar function

1. Its return type in `src/binder/binder.cpp`.
2. Its evaluation in `src/execution/expression_executor.cpp`.
3. A test in `test/unit/execution/`.

## Tests

Tests are doctest cases under `test/unit/<step>/`, all linked into
`slothdb_tests`.

## What zugodb vendors

zugodb copies `LICENSE`, `include/` and `src/`, and compiles them without
CMake; it copies nothing from `third_party/` since 0.4.0. A definition or
include path set only in CMake does not reach it, and a path moved, added
or removed there changes zugodb's build.

## Releases

A release is a `CHANGELOG.md` entry at the top of the file and a tag
`vX.Y.Z` on `main`; zugodb then moves its pin. A change to the database
file's format, an include path or a public name is a minor version.
