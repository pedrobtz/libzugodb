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
- Types the engine does not hold as DuckDB does are refused by name, not
  aliased (Stage 8): `DECIMAL` and `NUMERIC` (#26; they bound to `DOUBLE`
  and dropped precision and scale) until a fixed-point type exists, and
  `TIMESTAMP_S`, `TIMESTAMP_MS`, `TIMESTAMP_NS`, `TIMESTAMPTZ`, `TIMETZ`
  and `TIMESTAMP(p)` other than `p` of 4 to 6 (they were the engine's
  microsecond `TIMESTAMP` under another name). `DATETIME` stays, as
  DuckDB's alias. A decimal literal (`1.5`) stays a `DOUBLE`, where
  DuckDB types it `DECIMAL`: refusing it would refuse most arithmetic,
  and the cases where the two differ (`%` of a large integer by a
  decimal literal, `10.0 % 0.0`) are fixed or refused one by one.
- The plan text of `EXPLAIN` is the engine's own, not DuckDB's; the
  conformance runner leaves such queries out (L20).
- The engine holds no NaN (L23, until #42), and has no regular
  expressions until a host provides RE2's (L24, #40): both are errors,
  not approximations.

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
- **The runner** is a C++ program in this repository, `tools/slt/`,
  linked to the engine through `Connection::Query` (L1, L14). It replaced
  `bench/correctness/sqllogictest/` (removed in Stage 6), which ran
  SlothDB's published Python wheel against a live DuckDB over
  DataFusion's corpus and so never tested this tree.
- **Each case is one of:** passed; refused (the engine raised an error
  where DuckDB gave a result); wrong (a result that differs); accepted
  (the engine gave a result where DuckDB raised an error); crashed; hung.
  The bar is zero wrong, crashed and hung. Refused and accepted are
  counted, and accepted cases are read: each is a listed difference or a
  defect. A wrong or accepted case that follows a failed setup statement
  of its file is counted apart, as unreliable, and the cases after a crash
  or a hang as not run (L16).
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
(`bench/correctness/` went in Stage 6), the `llama.cpp`
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
- **L12 (2026-11-20, Stage 4, Claude's implementation call).** After the
  checksum, `Checkpoint::Load` accepts only what `Save` could have
  written, and reads it through one bounded reader: each fixed-width
  read, string length and count is checked against the bytes left before
  it is used or allocated, a count against bytes left over the least its
  element can take (a table 21 bytes, a column 5, a row one null flag per
  column). The header must hold Save's constants (block size 262144, block
  count 0, catalog and data offset both 4096, zeros from 48 to 4096), the
  tables must end at the end of the file, a table has at least one column
  and a name no other table has. Column types are those a table made by
  SQL can have (SQLNULL, BOOLEAN, TINYINT, SMALLINT, INTEGER, BIGINT,
  HUGEINT, FLOAT, DOUBLE, VARCHAR, BLOB, DATE, TIME, TIMESTAMP, INTERVAL,
  UUID); null flags are 0 or 1, BOOLEAN 0 or 1, SMALLINT (stored in 4
  bytes) in its range. Save writes non-NULL HUGEINT, INTERVAL and UUID
  values as text, which the loader never read back right (an INTERVAL
  could not be stored at all, a HUGEINT or UUID came back as garbage), so
  those are refused and only their NULLs load; a file that held such a
  value already held a wrong one. Amended the same day (recommended by an
  advisor sub-agent): SQL refuses the three types outright, in every
  database, in-memory included, in `Binder::ResolveTypeName`, the one place
  that made them ("HUGEINT is not supported"), so `CREATE TABLE`, `ALTER
  TABLE ... ADD COLUMN`, `CAST`, `TRY_CAST` and `::` fail and Save can no
  longer write a value the loader refuses; the loader's refusal stays as
  the backstop for files written before. A doctest makes each type name
  SQL accepts round-trip its least, greatest, zero and NULL through a
  close and reopen, so a type accepted with no file encoding fails the
  suite. Empty names, duplicate column names and
  any bytes in a name or string load, since SQL can make them. Each row
  group is allocated for the rows it gets, so the memory a load takes
  follows the file's size. Tables reach the catalog only after the whole
  file is read. The harness `fuzz/checkpoint_load.cpp` fixes catalog_size
  and the checksum of each input; its oracle is that a file that opens
  gives each table's row count by catalog, `count(*)`, `SELECT *` and a
  storage scan alike, and reopens after its save with the same rows. Reach
  is the edges of `src/storage/checkpoint.cpp` covered by the kept corpus,
  as `-print_coverage=1` reports them (`fuzz/run.sh`), against a floor in
  the workflow.
- **L13 (2026-11-20, Stage 4, recommended by an advisor sub-agent).** A
  string type's length, `VARCHAR(n)`, `CHAR(n)`, `TEXT(n)` or
  `STRING(n)`, is accepted and has no effect: the column is `VARCHAR`,
  `DESCRIBE` shows `VARCHAR`, and a longer string is stored whole. This is
  DuckDB's behaviour (its documentation: the length is for compatibility
  only), so it is a DuckDB-compatible behaviour for `docs/dialect.md`, not
  a listed difference. SlothDB enforced n, in bytes, on `INSERT ...
  VALUES` only (`INSERT ... SELECT` and `UPDATE` went around it), and the
  database file kept no length, so a reopened column was unbounded; L12's
  round-trip doctest found that loss. `VarcharTypeInfo` and
  `LogicalType::VARCHAR_N` go with the check, and with them a
  `std::stoull` that threw `std::out_of_range` on a length past 64 bits.
  DECIMAL and NUMERIC are not touched here: they bind to DOUBLE, filed for
  0.3.0.
- **L14 (2026-11-20, Stage 6, Claude's implementation call).** The
  runner, `tools/slt/`, is a library (`zugodb_slt_lib`, which the doctest
  suite links) and a program (`zugodb_slt`), built by default except on
  Windows (`SLOTHDB_BUILD_SLT`). It reads DuckDB's sqllogictest dialect
  as DuckDB's own runner does at the pinned tag (`test/sqlite/
  sqllogic_parser.cpp`, `sqllogic_test_runner.cpp`): records separated
  by empty lines, `statement ok|error|maybe`, `query <types> [nosort|
  rowsort|valuesort] [label]`, `hash-threshold`, `halt`, `mode skip` and
  `unskip` (other modes only print), `loop`, `foreach` with the groups
  `<signed>`, `<unsigned>`, `<integral>`, `<numeric>`, `<alltypes>`,
  `skipif` and `onlyif` with `duckdb` as the engine's name, `restart` and
  `load` with no path as a new in-memory database, and `require` where it
  holds for this runner on 64-bit Linux and macOS alike (`64bit`,
  `notwindows`, `notmingw`, `notmusl`, `nothreadsan`, `noforcestorage`,
  `skip_reload`, `no_alternative_verify`, `strinline`,
  `no_extension_autoloading`, `vector_size` up to DuckDB's 2048). A file
  is parsed and its loops expanded before anything runs, so the driver
  knows every case. It is left out, with the reason in `counts.tsv`, when
  it needs any other `require` (an extension, `longdouble`, `windows`,
  ...), another directive (`set`, `concurrentloop`, `include`, `unzip`,
  ...), a database file (`load <path>`), named connections, a condition
  on a loop variable, results in a file or a regex, or SQL that reads or
  writes files or attaches databases (the file table functions, a quoted
  path under `data/` or with a data file's extension, `COPY`, `ATTACH`,
  DuckDB's test directories, `INSTALL` or `LOAD`).
- **L15 (2026-11-20, Stage 6, Claude's implementation call).** A query's
  result is compared as DuckDB's runner compares it (`result_helper.cpp`):
  each value as DuckDB's cast to `VARCHAR` prints it (NULL as `NULL`, a
  boolean as `1` or `0`, the empty string as `(empty)`, a NUL byte as
  `\0`, a DOUBLE or FLOAT as DuckDB's vendored fmt 6.1.2 prints it, a
  date with ` (BC)`, a time with its trailing zeros cut, a blob with
  `\xNN` escapes), sorted by the sort mode, compared as a hash when the
  recorded value is one or the query has a label, otherwise value by
  value, one value per line or one row per line with tabs, with a column
  count other than the type string's a wrong result. The rule for a value
  that does not match as text depends on the engine's column: a boolean
  matches `true`, `false`, `1` or `0`; a FLOAT or DOUBLE matches any
  recorded number within DuckDB's tolerance (`ApproxEqual`: 1% of the
  value plus 1e-8); an integer matches the recorded text with spaces
  trimmed, or with a fractional part of zeros (`85.000000`), but not
  DuckDB's own DOUBLE text (`85.0`); anything else is wrong. This is
  DuckDB's comparison, which casts the recorded text to the result's type,
  except that an integer column does not take a fraction DuckDB's cast
  would round (`1.5` for 2) or its DOUBLE text. The type string itself is
  not checked, as there: the corpus writes `I` for many DOUBLE columns.
  A recorded row is split at tabs as DuckDB's `StringUtil::Split` does,
  dropping empty pieces (Stage 8, #38).
  So a type difference counts only where it changes the text past these
  rules, and a DECIMAL that the engine computes as a DOUBLE (#26) is
  wrong only where its value differs.
- **L16 (2026-11-20, Stage 6, Claude's implementation call).** How the
  cases of §5 are counted. A `statement ok` that fails is refused, and a
  `statement error` that succeeds is accepted; either may leave the
  database other than the file expects, so a later case of that file
  (until a `restart`) that would be wrong or accepted is counted as
  unreliable instead, with the line it follows; passed, refused,
  crashed and hung stay what they are. A `statement maybe` passes either
  way. A case is crashed when its child process dies during it, and hung
  when it makes no progress for 30 seconds or its process passes 2 GiB of
  resident memory (a statement that grows without end, as #23's); the
  cases after either, in that file, are not run. The committed counts are
  those of the build the `conformance` job uses: RelWithDebInfo with
  `-DSLOTHDB_SANITIZERS=ON`, so that a use after free is a crash every
  time and not by chance (a Release build crashed on different cases from
  run to run); leak checking is off in the runner, since a leak is not a
  case's outcome. The thread comparison is by class: the run at cap 2
  must give every case the class it gets at cap 1, and the cases that do
  not are committed in `threads.tsv` (a value read from uninitialised
  memory differs from run to run within a class, so the values are not
  compared). The counts are the run at cap 1. They are committed per
  platform, `test/conformance/linux-x86_64/` and `macos-arm64/`, the
  job's two, since the engine differs between them on three cases of the
  first counts, and the runner not at all: `trunc(0::FLOAT, 2)` returns a
  stale value on Linux, `-9223372036854775807::DECIMAL(19,0)::BIGINT`
  converts a double out of range (undefined, so by architecture), and
  `strftime` with `%-y` is the C library's.
- **L17 (2026-11-20, Stage 6, Claude's implementation call).** A
  `statement ok` that only sets one of DuckDB's execution settings, which
  its tests use to reach other code paths with the same answers, is
  neither run nor counted: `threads`, the verification and
  `debug_*` settings, the optimizer and join-threshold settings,
  `memory_limit`, profiling, progress bar, object cache and
  `explain_output` (the full list is `IsNoOpSetting` in
  `tools/slt/parser.cpp`). A setting that changes answers
  (`default_null_order`, `ieee_floating_point_ops`, `timezone`, ...) is
  run, and refused if the engine refuses it.
- **L18 (2026-11-20, Stage 7, Claude's implementation call).** How the
  first crashes and hangs are closed, where a choice was made. In a query
  with window functions, an `ORDER BY` item that is an ordinal or an
  output name sorts by that output column; any other is bound to the
  source columns, which the window operator then also emits after its
  select list, and a projection above the sort drops them; an item the
  planner cannot move to those columns (a window function or an aggregate
  not in the select list, a subquery) is refused. `INSERT ... SELECT`
  runs its `SELECT` to the end and copies each row before it appends the
  first, so the `SELECT` reads the table as it was (DuckDB's answer, #23),
  at the cost of holding the rows once. The binder checks the arity of
  the string functions DuckDB's tests call wrongly (`ASCII`, `LPAD`,
  `REPLACE`, `TRIM`, ... with DuckDB's signatures), and the executor
  checks every argument a scalar function reads by position, so a
  function not in the binder's list is an error rather than a read past
  its arguments. A string result past `UINT32_MAX` bytes, the most a
  `string_t` holds and DuckDB's limit, is refused before it is built
  (`LPAD`, `RPAD`).

- **L19 (2026-11-20, Stage 8, decided with an advisor).** Text to
  `DATE`, `TIME` and `TIMESTAMP` is read by DuckDB's grammar at the pinned
  tag (`Value::ParseDate`, `ParseTime`, `ParseTimestamp`): optional
  spaces, a year of any length and a sign, separators `-`, `/`, `\` or
  space used twice, one- or two-digit month and day checked against the
  month's length, an optional ` (BC)`, `epoch`; a time `H[...]:[MM[:[SS[.f]]]]`
  with hours to 24:00:00 and the fraction past microseconds cut; a
  timestamp as a date, then `T` or a space, a time and an optional `Z`; a
  `TIME` that is not a time is read as a timestamp and keeps its time of
  day, as DuckDB does. Each parse is Ok, Invalid or Unsupported. Invalid
  is text DuckDB rejects too: an error under `CAST` and NULL under
  `TRY_CAST`. Unsupported is text DuckDB may read but the engine cannot
  hold or does not take apart, and is an error under both, so a NULL is
  never a guess: `infinity` and `-infinity` (no infinite value), a year
  outside 1 to 9999 (the engine's date functions, `strftime`,
  `date_diff`, `date_trunc`, were written for those), a UTC offset or a
  time zone name, text after a date or a time. A date, a time and a
  timestamp print as DuckDB's (a year before 1 as its BC year with
  ` (BC)`, the fraction without trailing zeros, 24:00:00), the runner
  prints a TIME of 24:00:00 too, and a cast from a type DuckDB has no
  cast from (`INTEGER` to `DATE`) is an error under `TRY_CAST` as well.
- **L20 (2026-11-20, Stage 8, decided with an advisor).** A query whose
  expected result holds `<REGEX>:` or `<!REGEX>:` in any column is left
  out of its file alone and counted in `totals.tsv` (`queries left out:
  regex results`). DuckDB's runner matches the regex against DuckDB's
  own text, mostly an `EXPLAIN` plan, which no other engine gives; L14
  looked in the first column only, so `EXPLAIN` queries ran and counted
  as wrong. The query is left out rather than its file because a query
  changes nothing the later cases read. Amends L14.
- **L21 (2026-11-20, Stage 8, decided with an advisor).** The window
  operator computes `ROW_NUMBER`, `RANK`, `DENSE_RANK`, `NTILE`, `LAG`,
  `LEAD`, `FIRST_VALUE`, `LAST_VALUE`, `SUM`, `COUNT`, `AVG`, `MIN` and
  `MAX`, over a column, with SQL's default frames. The binder refuses,
  as an allow-list, every other shape: another function, `DISTINCT`,
  `FILTER`, an argument other than a column (an integer constant for
  `NTILE` and `LAG`'s offset; `LAG`'s default is refused), an `ORDER BY`
  key other than a column, more than one key for `RANK`, `DENSE_RANK`
  and the aggregates (peers were found by the first key), `LAST_VALUE`
  with `ORDER BY` (its frame ends at the current row's last peer, which
  is not computed), and `SUM` or `AVG` of a non-numeric column. The
  planner refuses a running aggregate beside windows over another
  partition or order (that path took the whole partition) and a `QUALIFY`
  other than one window compared with a constant (it was not applied).
  Window aggregates over integers are summed in 64 bits; one past
  `BIGINT` is refused (DuckDB's is a `HUGEINT`).
- **L22 (2026-11-20, Stage 8, decided with an advisor).** DuckDB's string
  functions count code points (`SUBSTRING`, `LEFT`, `RIGHT`, `INSTR`,
  `STRPOS`, `TRANSLATE`, `LIKE`'s `_`; a `BLOB`'s `SUBSTRING` counts
  bytes), bytes (`STRLEN`) or grapheme clusters (`LENGTH`, `REVERSE`), and
  map case with utf8proc's tables (`UPPER`, `LOWER`, `ILIKE`). The engine
  counts code points and bytes as DuckDB does. It has no grapheme rules:
  `LENGTH` and `REVERSE` work where every code point is surely a cluster
  of its own (printable ASCII, Latin-1 to Latin Extended-B, Greek and
  Cyrillic letters, CJK ideographs, Hangul syllables, emoji pictographs
  without modifiers, no `\r\n`) and are refused otherwise. It has no case
  tables yet: `UPPER` and `LOWER` of non-ASCII text are refused, and so is
  `ILIKE` with a non-ASCII pattern or a string holding U+0130 or U+212A
  (whose lowercase is ASCII); an ASCII pattern against other text is
  exact. The interim, per the advisor, is refusal, not a partial table; a
  complete simple case mapping generated from UnicodeData is a
  sub-issue of v0.3.0.

- **L23 (2026-11-20, Stage 8, decided with an advisor).** What the
  engine holds, and what a statement returns.
  - No NaN. DuckDB orders NaN above every number and equal to itself;
    the engine's comparisons are IEEE's in many hand-written paths
    (sorts, joins, groups, `MIN`/`MAX`), and teaching each DuckDB's order
    would leave any path missed a silent wrong answer. So a NaN is an
    error wherever it would be made: an expression's result (text
    `'nan'`, `0/0`, `inf - inf`, `SQRT(-1)`, `x % 0` of doubles, under
    `TRY_CAST` too; `ExpressionExecutor::Execute` checks every `FLOAT`
    and `DOUBLE` it computes), a table's append (`DataTable::Append`,
    so `Connection::Append` too), a database file's value
    (`Checkpoint::Load`; a file written before 0.3.0 that holds one is
    refused), and a query's result, the backstop for an aggregate over
    infinities. Infinities are kept, since IEEE orders them as DuckDB
    does, with DuckDB's values: `x / 0` and `POWER(0, -2)` are
    infinities. The R package decides what `dbWriteTable` of an R `NaN`
    is (an error from the engine now). #42 tracks DuckDB's NaN
    semantics, which would lift this refusal.
  - DuckDB's statement results. `CREATE TABLE AS` returns one row,
    `Count`, the rows it made; `CREATE TABLE` a `Count` column and no
    row; the other DDL is left as it was until a recorded case shows
    its shape. `QueryResult::is_count` marks a result of `INSERT`,
    `UPDATE`, `DELETE` or `CREATE TABLE`, so a caller classifies by it
    rather than by parsing SQL or trusting a column's name.
- **L24 (2026-11-20, Stage 8, the maintainer's decision).** Every regular
  expression function and operator is refused, at bind time or before
  (`regexp_matches`, `regexp_full_match`, `regexp_replace`,
  `regexp_extract`, `regexp_extract_all`, `regexp_split_to_array`,
  `string_split_regex`, `regexp_escape`, `REGEXP`, `REGEXP_LIKE`,
  `REGEXP_MATCH`, `SIMILAR TO`, `~`, `!~`), with one error naming #40,
  until a host-supplied provider gives them RE2's semantics (#40,
  zuregex#211). `std::regex`, ECMAScript over bytes, gave answers that
  were not DuckDB's and is gone. A Pike VM over a subset of RE2 was built
  for this stage on an advisor's recommendation and dropped by the
  maintainer: the package family has one regex engine, zuregex's RE2,
  and no second implementation to maintain.

- **L25 (2026-11-20, the maintainer's decision, on an advisor's
  recommendation).** The engine stays an R-free library and gets what it
  does not implement from its host. A provider interface (Stage 11.1, #40)
  takes one versioned C table per service: regex, UTF-8, and later UUID and
  random numbers. The engine refuses a function whose table is absent or
  of the wrong version, so the counts here, which run with no provider, do
  not change. zugodb fills the tables from zuregex, zuutf, zuid and zurand
  at load, and tests what depends on them. Provider functions run on
  worker threads: they call no R API and return a status. Dates and
  compression are not providers; the engine's codecs go with its readers
  in Stage 11. The plan across the package family is
  [pedrobtz/packages `zugodb.md`](https://github.com/pedrobtz/packages/blob/main/zugodb.md).

## 9. Open questions

- **LQ1 (settled 2026-11-20, Stage 6).** Which DuckDB tag is pinned, and
  which directories of `test/sql` make the first subset? DuckDB `v2.0.0`
  (commit `d54cbbe3df17e8e5ce68312f483b4155836112aa`, released
  2026-10-21), the newest release at least two weeks old
  (`test/conformance/duckdb-tag`). The subset is every `.test` file (not
  `.test_slow`) under `aggregate`, `filter`, `join`, `order`, `subquery`,
  `window`, `types/{blob,date,decimal,float,null,numeric,string,time,
  timestamp}` for the types of [zugodb §6][zdesign], `function/numeric`,
  `function/string` and `function/date`: 809 files
  (`test/conformance/subset.txt`), of which the runner leaves 63 out by
  L14's rules and runs 746, with 20,461 cases. The counts are in
  `test/conformance/<platform>/totals.tsv` and `counts.tsv`; the job runs
  under ten minutes (about 3.5 on Linux and 7 on macOS, build included), so the
  subset is not cut further.
- **LQ2.** When the engine's readers are cut, what else in `src/storage/`
  still needs zstd, snappy or miniz? [zugodb §7.5][zdesign] says the cut
  is sized first; the sizing is the first work of that stage.

[zdesign]: https://github.com/pedrobtz/zugodb/blob/main/.agents/design.md
