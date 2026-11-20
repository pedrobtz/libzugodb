# libzugodb: design

Status: draft 2026-11-19. The engine's own design, for what is decided in
this repository. The R package's design,
[zugodb `.agents/design.md`][zdesign], stays the specification for what the
two share: the reasons for the fork (D1, D9), files and readers (§7),
persistence (§8), the release bar (Q18) and extensions (§16). Where the two
disagree, that one wins and this one is corrected.

The order of work and its exit criteria are in [roadmap.md](roadmap.md).

## 1. What it is

A small embedded columnar SQL engine in C++20: SlothDB at `8180f21`
(v0.2.7+) with the changes in `CHANGELOG.md`. It has one consumer, the R
package zugodb, which vendors a tagged release and builds it from a
generated `Makevars` (§7).

Its promise is the one zugodb makes for it: it may refuse a query, it never
gives a wrong answer, and it never takes the host process down.

## 2. Scope

In scope: SQL over tables held in memory, a database file that survives a
restart, rows appended through the C++ API, and results read back.

Not in scope, and refused or removed:

- Reading or writing files from SQL. Files reach the engine as Arrow C
  streams, through zugodb ([zugodb §7][zdesign]).
- Any network access, and loading any code.
- Transactions with undo, and cancellation of a running query.
- Other bindings. SlothDB's Python package, its npm, Homebrew, deb, rpm and
  Arch packaging, its WASM build and its website are not maintained here
  (§6).
- Speed as a goal. SlothDB's benchmark claims are SlothDB's; nothing here
  repeats them until it is measured here.

## 3. Dialect

**A documented subset of DuckDB SQL.** Not DuckDB SQL: nested types,
lambdas, `PIVOT`, macros and `ASOF` joins are far beyond an engine of
44,000 lines.

The rule, for every statement:

1. If the engine accepts it, the result is DuckDB's, value for value and
   type for type, or the difference is listed in `docs/dialect.md` with its
   reason.
2. Otherwise it is an error. An error is never a defect by itself.

What follows from it:

- DuckDB is the oracle, through recorded answers (§5), not a link-time or
  test-time dependency.
- A form that another dialect spells the same way with another meaning
  follows DuckDB or is refused. SlothDB added MySQL forms
  (`DATEDIFF(d1, d2)`, `DATE_SUB`, `SUBDATE`, `TIMESTAMPADD`,
  `DATE_ADD(date, n, unit)`): each is kept only where DuckDB accepts the
  same text with the same result, and refused otherwise (L4).
- `docs/dialect.md` is the user's reference: statements, types, functions
  and the listed differences. It is written from the conformance counts
  (§5), not from SlothDB's `SQL_REFERENCE.md`, which describes what
  SlothDB meant to accept.
- New syntax is added only with a recorded DuckDB answer for it.

## 4. Parser

**The hand-written tokenizer and recursive-descent parser stay**, and are
hardened (L2).

Why not an existing parser:

- The parser is about 2,400 of 44,000 lines. The fixes SlothDB logged from
  its conformance runs were mostly casts, expressions and set operations in
  the binder and the executor. A new parser would leave those where they
  are.
- A full parser accepts far more than the engine can run, so a layer that
  refuses most of it would still be written, and the binder and
  `connection.cpp`, which both walk today's syntax tree, would be
  rewritten against another tree.
- The candidates fit badly. libpg_query is large generated C, a real cost
  for code vendored into an R package, and has no DuckDB syntax
  (`QUALIFY`, `EXCLUDE`). DuckDB's parser is not separable from DuckDB.
  sqlparser-rs needs a Rust toolchain. Hyrise's sql-parser is small and
  limited.

What hardening means:

- Quoted identifiers read the `""` escape ([zugodb §5][zdesign] records
  the gap).
- Every statement either parses to a tree the binder handles or is a
  parser error with a position; nothing is skipped silently.
- A fuzz harness over SQL text, with the oracle of §5.
- Table function names leave the parser, the tokenizer and
  `connection.cpp` for one registry, when table sources arrive
  ([zugodb §16][zdesign]); not before.

The question is reopened if a sibling package ends up with a parser the
engine could link.

## 5. Conformance and its oracles

The bar is [zugodb's Q18][zdesign]: on a pinned corpus no case gives a
wrong answer, crashes or hangs. The pass rate is reported and not
targeted.

- **The corpus** is DuckDB's own `test/sql` files, in sqllogictest format,
  at one pinned DuckDB release tag (L3). Each case carries DuckDB's
  recorded result, so no DuckDB is needed where the tests run, and the
  corpus is the dialect of §3 by construction. A committed list names the
  files in the subset; a file that needs a directive the runner does not
  implement, an extension, or a file on disk is left out and counted as
  left out.
- **The runner** is a C++ program in this repository, linked to the
  engine through `Connection::Query` (L1). It replaces
  `bench/correctness/sqllogictest/`, which ran SlothDB's published Python
  wheel against a live DuckDB over DataFusion's corpus and so never
  tested this tree.
- **Each case is one of:** passed; refused (the engine raised an error
  where DuckDB gave a result); wrong (a result that differs); accepted
  (the engine gave a result where DuckDB raised an error); crashed; hung.
  The bar is zero wrong, crashed and hung. Refused and accepted are
  counted, and accepted cases are read: each is a listed difference or a
  defect.
- **Isolation.** One file runs in one child process under a time limit,
  so a crash or a hang costs that file's remaining cases and nothing else.
- **Threads.** The subset runs with the thread cap at 1 and at 2, and the
  results must be equal.
- **Fuzzing** has no recorded answer, so its oracles are weaker: no crash,
  hang or sanitizer report, and the same result at one thread and two.

The doctest suite stays as the place for a regression test of each fix.

## 6. What the repository holds

Kept: `include/`, `src/`, `test/`, `cmake/`, `third_party/` as far as the
build uses it, `tools/shell/` (a development tool, not a product),
`docs/SLOTHDB-README.md` (the credit the README links), `LICENSE`,
`CHANGELOG.md`.

Removed, as SlothDB's product and not the engine: the website and
playground under `docs/`, `packaging/`, `python/`, `cloudflare/`,
`assets/`, `install.sh`, `scripts/build-wasm.sh`, `tools/playground/`,
`examples/`, the performance benchmarks under `bench/`
(`bench/correctness/` stays until Stage 6), the `llama.cpp`
submodule with `SLOTHDB_ASK_MODEL`, the `SLOTHDB_EDGE` and
`SLOTHDB_BUILD_SHARED` options, and `TODO-2026-04-29.md`. Git history
keeps all of it.

`src/api/`, SlothDB's C API for its Python binding, goes with the binding
(L5). The public C header that extensions will use
([zugodb §16.3][zdesign]) is a new one.

No large file is split or reordered before the bar of §5 holds:
`physical_planner.cpp` is 14,600 lines of special cases, and a
restructuring with no measure of correctness hides regressions.

## 7. The contract with zugodb

zugodb copies `LICENSE`, `include/`, `src/` and, from `third_party/`, the
zstd decompressor, `fastpforlib` and `snappy`, and compiles them without
CMake. So:

- a change under those paths reaches users only by a release tag here and
  a new pin there;
- a path moved, added or removed there is a change to zugodb's
  `tools/vendor.yaml`, and is named in the release's changelog entry;
- nothing may depend on a definition or include path that only CMake sets.

Releases are tags `vX.Y.Z` on `main` with a `CHANGELOG.md` entry. A change
to the database file's format, to an include path or to a public name is a
minor version.

## 8. Decisions

- **L1 (2026-11-19).** The conformance runner is C++ against
  `Connection::Query`, and the Python binding is not kept. Proposed by
  Claude; the maintainer said to go ahead.
- **L2 (2026-11-19).** The hand-written parser stays (§4). Proposed by
  Claude; the maintainer said to go ahead.
- **L3 (2026-11-19, proposed by Claude, agreed by the maintainer).** The corpus is
  DuckDB's `test/sql` at a pinned tag (§5), not DataFusion's corpus against
  a live DuckDB, as SlothDB's harness used, nor SQLite's sqllogictest
  corpus, whose dialect is SQLite's. zugodb's Stage 8 says "the corpus's
  own expected results", which this meets.
- **L4 (2026-11-19, proposed by Claude, agreed by the maintainer).** MySQL forms
  follow DuckDB or are refused (§3).
- **L5 (2026-11-19, proposed by Claude, agreed by the maintainer).** `src/api/` is
  removed with the Python binding (§6).
- **L6 (2026-11-19).** The repository is cleaned by deletion first, and
  nothing is restructured before the bar holds (§6). Proposed by Claude;
  the maintainer said to go ahead.

- **L7 (2026-11-19, proposed by Claude, agreed by the maintainer).** No DuckDB runs in CI: the
  corpus carries its answers. Where an answer must be recorded for SQL the
  corpus lacks (a doctest for a fix, a listed difference), it comes from
  DuckDB's Python package, pinned with `duckdb==` to the corpus's tag, by
  a script under `tools/` that writes the answer into the test. Python and
  not R: the wheel installs in seconds at an exact version, where R's
  `duckdb` compiles from source on Linux and this repository has no R.
  zugodb keeps R's `duckdb` for its own recorded answers
  (`tools/oracle.R`).
- **L8 (2026-11-20, recommended by an advisor sub-agent, as the
  maintainer asked for open decisions).** Every performance benchmark
  under `bench/` goes (`run.py`, `check.py`, `clickbench/`, `sloth16/`,
  `taxi/`): speed is not a goal (§2), they time SlothDB's own build against
  DuckDB, and they read files from SQL, which Stage 2 refuses.
  `bench/correctness/` stays until Stage 6 replaces it. `examples/` goes
  too: every command in it reads or writes a file from SQL, SlothDB's
  product demo, and nothing in the build or tests reads it. The
  `#ifdef SLOTHDB_EDGE` blocks in `src/` stay with the option gone, since
  removing them would change what zugodb vendors.
- **L9 (2026-11-20, Stage 1, Claude's implementation call).** An
  exception on a worker thread is kept by one mechanism in
  `include/slothdb/common/parallel.hpp`: `WorkerErrors` (the first
  `std::exception_ptr`, under a mutex, with an atomic flag workers poll to
  stop early) and `WorkerGroup` (threads started and joined in one scope,
  rethrow after the join, a thread that fails to start is an error of the
  group). Every new thread start uses one of them or `ParallelFor`, and
  the doctest "every thread start in src/ is a covered kind" fails on any
  other. The test seam is a process-wide atomic armed only by the tests,
  so it reaches zugodb's build with no CMake definition. Parquet fixtures
  for tests are written by DuckDB's Python package, pinned
  (`duckdb==1.1.3`) in `tools/make_parquet_fixtures.py`, as L7 allows;
  the pin moves to the corpus's tag when LQ1 settles it.
- **L10 (2026-11-20, Stage 2, Claude's implementation call).** Files from
  SQL are refused in the parser, where each route's table reference or
  statement is built, so the refusal holds at any depth (subqueries,
  CTEs, joins, views, `EXPLAIN`, `INSERT ... SELECT`) without a walk over
  the parsed tree, before any statement of the batch runs and before
  anything is opened. One `NotImplementedException`: "Reading or writing
  files from SQL is not supported: <route>". The list of file table
  functions holds DuckDB's names as well as the ones the engine read, so
  each meets the refusal and not a syntax error; a table may still be
  called `read_csv`. `CREATE LIVE VIEW`, which only ever watched a file,
  is refused with them. Statements the parser never accepted (`ATTACH`,
  `EXPORT DATABASE`, `IMPORT DATABASE`, `INSTALL`, `LOAD`) stay syntax
  errors. Doctests of the Parquet scan operator, including Stage 1's,
  reach it through a table set up with the catalog's C++ API
  (`test/unit/parquet_table.hpp`, what `read_parquet()` did), which goes
  with the reader in Stage 11.
- **L11 (2026-11-20, Stage 3, recommended by an advisor sub-agent, as the
  maintainer asked for open decisions).** The database file is format
  version 1001 ([zugodb §8 and Q7][zdesign]). Its 48-byte header keeps
  upstream's layout (magic `BDLS` on disk, version, block size, then
  catalog offset and size, `total_blocks` and data offset, 8 bytes each),
  except that upstream's reserved field at offset 12 holds a CRC-32 of the
  whole file with those 4 bytes read as zero; bytes 48 to 4095 are zero
  and the tables start at 4096. The CRC is zlib's (reflected 0xEDB88320),
  in `include/slothdb/common/crc32.hpp`, not miniz's `mz_crc32`, which
  leaves with the readers. `Checkpoint::ComputeChecksum` computes it over a
  whole file image; Save stores it after reading back the `.tmp` file, and
  Load and the fuzz harness use the same function. Opening checks, in
  order: the file holds a header, the magic, the version (any other than
  1001 is refused naming both, so the header stays readable before the
  checksum and an upstream file is told by its version), the size equals
  catalog offset plus catalog size (compared by subtraction, which also
  refuses appended bytes), then the checksum, then the tables. Format
  1001 is little-endian: the code copies integers in host order and
  refuses on a big-endian host at run time, not with `#error`, so the
  rest of the engine still builds there. The golden file
  `test/fixtures/golden_v1001.db` is written by `tools/make_db_fixtures.sh`
  through the shell; a layout change without a version bump fails its
  test.

## 9. Open questions

- **LQ1.** Which DuckDB tag is pinned, and which directories of `test/sql`
  make the first subset? Proposal: the newest release at the time, and the
  directories that need no extension and no file: `aggregate`, `filter`,
  `join`, `order`, `subquery`, `window`, `types` for the types of
  [zugodb §6][zdesign], `function/numeric`, `function/string` and
  `function/date`. Settled in the runner's stage, with the counts.
- **LQ2.** When the engine's readers are cut, what else in `src/storage/`
  still needs zstd, snappy or miniz? [zugodb §7.5][zdesign] says the cut
  is sized first; the sizing is the first work of that stage.

[zdesign]: https://github.com/pedrobtz/zugodb/blob/main/.agents/design.md
