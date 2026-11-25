# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

libzugodb is a small embedded columnar SQL engine in C++20: SlothDB at commit
`8180f21` (v0.2.7+), forked and maintained here as its own project. Its one
consumer is the R package [zugodb](https://github.com/pedrobtz/zugodb), which
vendors a tagged release of this repository and compiles it at install.

[.agents/design.md](.agents/design.md) is the engine's design (dialect,
parser, conformance, what the repository holds) and
[.agents/roadmap.md](.agents/roadmap.md) its stages, one pull request and
one issue each. The R package's `.agents/design.md` is the specification
for what the two share (decisions D9 to D12, §7 on readers, §16 on
extensions, Q18 on the release bar) and wins where they disagree. Read both
before changing what the engine accepts or refuses.

## Commands

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
build/test/zugodb_tests                    # the whole doctest suite
build/test/zugodb_tests -tc="*Window*"     # test cases by name
build/test/zugodb_tests -sf="*test_joins*" # one source file
build/test/zugodb_tests -ltc               # list test cases
```

- `-DZUGODB_SANITIZERS=ON` builds with ASan and UBSan (a CI job).
- `-DZUGODB_TSAN=ON` builds with ThreadSanitizer (the `linux-tsan` job),
  not with `ZUGODB_SANITIZERS` or `ZUGODB_FUZZ`. Run the suite with
  `TSAN_OPTIONS="halt_on_error=1 second_deadlock_stack=1"`;
  `ZUGODB_THREAD_ROUNDS=n` repeats the multi-threaded loop of
  `test/unit/execution/test_threads.cpp`. Apple clang has TSan. A kernel
  with high mmap randomisation stops clang 18's TSan at start; the CI job
  sets `vm.mmap_rnd_bits=28`.
- `-DZUGODB_BUILD_SHELL=ON` adds the `zugodb` shell from `tools/shell/`.
- The conformance runner `zugodb_slt` (`tools/slt/`, design §5 and L14 to
  L17) is built by default except on Windows (`-DZUGODB_BUILD_SLT=OFF`
  to skip it). The subset is DuckDB's `test/sql` at the tag in
  `test/conformance/duckdb-tag`:

  ```sh
  test/conformance/fetch.sh                  # corpus into test/conformance/corpus/
  cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DZUGODB_SANITIZERS=ON
  cmake --build build-asan -j8 --target zugodb_slt
  test/conformance/run.sh build-asan/tools/slt/zugodb_slt           # compare with the committed counts
  test/conformance/run.sh build-asan/tools/slt/zugodb_slt --update  # rewrite them
  build/tools/slt/zugodb_slt file test/conformance/corpus/test/sql/join/test_join.test   # one file: each case that did not pass
  build/tools/slt/zugodb_slt file <file> --in-process --cap 2       # in this process, for a debugger
  ```

  The committed counts (`counts.tsv`, `totals.tsv`, `failures.tsv`,
  `threads.tsv`) are per platform, `test/conformance/linux-x86_64/` and
  `macos-arm64/`, the `conformance` job's two, from its ASan and UBSan
  build; elsewhere set `CONFORMANCE_PLATFORM` to compare with one. A
  change to them is committed with the change that makes it (the job's
  artifact holds each platform's run).
  `CONFORMANCE_OUT=dir` keeps a run's outputs, with `details.tsv` (why
  each case did not pass).
- `-DZUGODB_FUZZ=ON` (clang with libFuzzer; Apple clang lacks it) builds
  the harnesses in `fuzz/`; `fuzz/run.sh checkpoint_load 120
  src/storage/checkpoint.cpp <floor>` runs one as the `fuzz` CI job does.
- Sources are globbed: after adding a `.cpp`, re-run the configure step.
- Warnings are `-Wall -Wextra -Wpedantic` (`/W4 /WX` under MSVC).

CI (`.github/workflows/ci.yml`) runs the suite on Linux gcc, Linux with
ASan and UBSan, Linux with ThreadSanitizer, macOS clang, and MinGW-w64
UCRT64, the toolchain of R's Rtools. A change must pass all five, the
`fuzz` job: libFuzzer over
the loader, 120 s per pull request and an hour weekly, with a floor on
its reach, and the `conformance` job on Linux and macOS: the subset's
counts must equal the committed ones.

## Names

Everything is named `zugodb` since 0.4.0 (Stage 12, design L31): the
namespace, the `include/zugodb/` include path, the `ZUGODB_*` macros and
CMake options, the `zugodb` library target (`build/src/libzugodb.a`), the
`zugodb_tests` target and the shell, whose target is `zugodb_shell` and
whose executable is `build/src/zugodb`. The database file keeps SlothDB's
magic (`ZUGODB_MAGIC`, the bytes `BDLS`) and its format version 1001.

SlothDB's name is left only in the credit (`LICENSE`, the README's origin
paragraph and `docs/SLOTHDB-README.md`, SlothDB's own README, which is not
this repository's documentation), the history (`CHANGELOG.md`, `.agents/`)
and prose about the origin, written "SlothDB". `tools/check_names.sh`, a
step of the `test` jobs, refuses the name in lower or upper case anywhere else.

## Architecture

A query goes through one pipeline, one directory per step, with headers
mirrored under `include/zugodb/`:

`parser/` (hand-written tokenizer and recursive-descent parser, to
statement and expression trees) → `binder/` (names and types) → `planner/`
(logical operators) → `optimizer/` (a few passes, about 230 lines) →
`execution/` (physical operators that share one pull interface,
`PhysicalOperator::GetData(DataChunk &)`).

The public C++ API is `Database` and `Connection` in
`include/zugodb/main/`; `Connection::Query(sql)` returns a materialised
`QueryResult`. There is no C API: SlothDB's, which served its Python
binding, is removed.

Where the weight is, which the directory names do not show:

- `src/execution/physical_planner.cpp` (5,900 lines) holds the physical
  operators and many special-case fast paths for specific query shapes; the
  other files in `execution/` (`count_group_agg`, `literal_emit_filter`,
  ...) are such paths split out of it.
- `src/execution/expression_executor.cpp` (5,100 lines) is every scalar
  function and cast. A new function also needs its entry in the function
  registry of `src/binder/binder.cpp` (arity, argument and return types;
  any name outside it is refused) and in `docs/dialect.md`, which
  `tools/check_dialect.sh` holds to the registry in CI.
- `src/main/connection.cpp` (2,300 lines) does far more than dispatch:
  DDL, DML, `generate_series`, views, CTEs and several rewrites of the
  parsed statement happen here before or instead of the pipeline.
- SQL reads and writes no file (design §2). The parser refuses every route
  before anything is opened: the file table functions (`read_csv`,
  `read_parquet`, `read_json`, ...), a path used as a table, `DESCRIBE` of
  either, `COPY` and `CREATE LIVE VIEW` (`RefuseFileAccess` in
  `src/parser/parser.cpp`). Nothing is behind the refusals: the engine has
  no file reader since Stage 11 (design L28), and files reach it as Arrow
  C streams through zugodb.
- `src/storage/` is the table storage: in memory, with a checkpointed
  database file. It compresses nothing.
- The database file is format version 1001 with a CRC-32 over the whole
  file (`Checkpoint::ComputeChecksum`; layout in design L11). A change to
  what `Checkpoint::Save` writes bumps `ZUGODB_VERSION` in the same
  commit, with a new golden file (`tools/make_db_fixtures.sh`) and a new
  fuzz corpus (`fuzz/make_corpus.sh`). `Checkpoint::Load` reads through a
  bounded reader and accepts only what Save could write (design L12): a
  new field read from the file gets its bound and a doctest at the limit
  and one past it.

## Invariants

These are what the fork changed, and the R package's checks fail if one
breaks:

- **Refuse, never guess.** Where the engine cannot give the right answer it
  raises an error. A wrong value with no error is the worst outcome; an
  error is acceptable and counted.
- **Offline, and loads no code.** Remote paths (`http://`, `https://`,
  `s3://`) are errors. No HTTP client, extension loader or model download.
- **Quiet.** Nothing writes to `stdout` or `stderr`; nothing calls `abort`,
  `exit` or `rand`. The R package inspects the shared object's imports.
- **Bounded threads.** Every thread pool respects the process-wide cap in
  `include/zugodb/common/parallel.hpp` (`SetThreadCap`).
- **Builds with MinGW-w64**, not only MSVC, on Windows.
- **No transactions.** `BEGIN`, `COMMIT` and `ROLLBACK` are errors: every
  statement commits as it runs and nothing is undone (design L26).

## What the R package takes

`tools/vendor.yaml` in zugodb copies `LICENSE`, `include/` and `src/`
(without its CMake file); nothing from `third_party/` since Stage 11, which
removed zstd, `fastpforlib` and `snappy` (`tools/check_symbols.sh
build/src/libzugodb.a` keeps them out). It builds them from a generated
`Makevars`, not from CMake, with `-I` on `include/`, so the glue includes
`"zugodb/main/connection.hpp"` and the like; so:

- moving, renaming or adding a directory under `src/` or `include/`
  changes the R package's build;
- a compile definition or include path set only in CMake does not reach it.

An engine change reaches the R package as a pull request here, a release
tag, and a new pin there. Each release gets an entry at the top of
`CHANGELOG.md`; the entries below the "SlothDB" heading are upstream's.

## Left over from SlothDB

SlothDB's `EDGE` blocks (`#ifdef`) went with the code around them in Stage 11,
and the comments that described SlothDB's C API in Stage 12. The CUDA and
Metal stubs in `src/execution/gpu/` are still compiled, behind
`ZUGODB_CUDA` and `ZUGODB_METAL`, which nothing defines.
