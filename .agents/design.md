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
- NaN, the infinities and -0.0 are DuckDB's (L30; L23 refused NaN until
  #42), and the engine has no regular expressions until a host provides
  RE2's (L24, #40): an error, not an approximation.

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
`physical_planner.cpp` is 5,900 lines of special cases (14,600 before the
readers went, L28), and a
restructuring with no measure of correctness hides regressions.

## 7. The contract with zugodb

zugodb copies `LICENSE`, `include/` and `src/`, and compiles them without
CMake; since Stage 11 it copies nothing from `third_party/` (L28), which
held the zstd decompressor, `fastpforlib` and `snappy` until then. So:

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
  - No NaN (superseded by L30, which gives NaN DuckDB's semantics and
    lifts each refusal below). DuckDB orders NaN above every number and equal to itself;
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

- **L26 (2026-11-20, Stage 9, decided with an advisor).** What the parser
  and binder accept, and how it is written down.
  - Identifiers. `"..."` and DuckDB's `` `...` `` delimit a name, a doubled
    delimiter is one, an empty one is an error. Names of tables, views,
    columns, CTEs and aliases match ignoring ASCII case, quoted or not, and
    keep their spelling; a bare column's result name is the catalog's. Two
    names equal under that rule are refused in `CREATE TABLE`, `ALTER
    TABLE` and one `FROM`; in a subquery, a view or `CREATE TABLE AS` the
    later one becomes `name_1`, `name_2`, ... as DuckDB's does
    (`SchemaCatalogEntry::CreateTable`). `Checkpoint::Load` refuses a file
    with two such table or column names, or an empty one: SQL can make
    neither (amends L12).
  - Statements. A statement ends at `;` or the end of the text; a token
    after a complete statement is a parser error. Every parser error's
    message ends with its line and column. A CTE's column aliases name its
    columns. `BEGIN`, `COMMIT` and `ROLLBACK` are refused, the batch before
    any of it runs: a statement after a failed one inside `BEGIN ... COMMIT`
    was kept where DuckDB rolls the transaction back, and no undo can fix
    that. `PRAGMA database_list`, `SHOW ... LIKE` and `SHOW COLUMNS` are
    refused; `DESCRIBE`, `PRAGMA table_info` and `SHOW TABLES` give DuckDB's
    rows.
  - The function registry. One list in `src/binder/binder.cpp`, between
    `BEGIN` and `END FUNCTION REGISTRY`, holds every scalar function (with
    DuckDB's argument counts), aggregate and window function the engine
    computes; any other name is an error at bind time, where it bound as an
    `INTEGER` and failed per row or gave a value DuckDB never gives.
    `docs/dialect.md` lists the same names, `tools/check_dialect.sh` (run by
    the `ci` jobs) compares the two, and a doctest calls each one. The
    binder checks the argument types of the text, number and date functions
    against DuckDB's (a `BOOLEAN` read as a `string_t` overflowed a heap
    buffer).
  - Refused as not DuckDB's (L4): `NVL`, `IIF`, `GETDATE`, `CURDATE`,
    `CURTIME`, `FROM_UNIXTIME`, `UNIX_TIMESTAMP`, `DATE_FORMAT`,
    `FORMAT_TIMESTAMP`, `DATEADD`, `TIMESTAMPADD`, `TIMESTAMPDIFF`,
    `SUBDATE`, `ADDDATE`, `BASE64_ENCODE`, `BASE64_DECODE`, `INITCAP`,
    `SHA512`, `SHA2_256`, `SHA2_512`, `SHA_1`, `RAND`, `TRUNCATE()`,
    `STR_TO_DATE` and `DATEDIFF(d1, d2)`. As a provider's (L25): `RANDOM`,
    `SETSEED`, `UUID`, `GEN_RANDOM_UUID`. As a type or zone the engine does
    not hold: `NOW`, `CURRENT_TIMESTAMP`, `CURRENT_DATE` (DuckDB's are in
    the session's time zone), `TO_TIMESTAMP`, `AGE`, `TIMESTAMP ± INTEGER`,
    `TIMESTAMP - TIMESTAMP` (an `INTERVAL`), `DATE + BIGINT`. As not
    DuckDB's values: `VERSION`, `CURRENT_DATABASE`. `DATE_ADD` and
    `DATE_SUB`, whose DuckDB forms take an `INTERVAL` or count complete
    parts, are refused rather than reimplemented.
  - Kept and made DuckDB's: `DATE_DIFF` and `DATEDIFF` are `(part, start,
    end)` and count the part's boundaries with each operand cut down to
    the part (floor division before 1970), for twelve parts; `STRFTIME` is
    DuckDB's specifiers formatted by the engine, not the C library's; a
    condition (`AND`, `OR`, `NOT`, `WHERE`, `HAVING`, `QUALIFY`, `ON`,
    `CASE WHEN`) of another type is cast to `BOOLEAN`; `CASE`, `COALESCE`,
    `NULLIF`, `GREATEST` and `LEAST` keep one type as it is, widen numbers,
    give a string literal the other branch's type, and refuse any other
    mix. Each was checked against DuckDB's Python package on generated
    inputs (several hundred values or pairs each).
  - The SQL fuzzer, `fuzz/sql.cpp`, runs each input against a fixed schema
    (two tables with every SQL type and NULLs, a view) in a new database at
    thread caps 1 and 2; its oracle is the same class (a result or an
    error) and, for a result, the same row and column counts. Values are
    not compared, as L16 compares classes. Its seeds are 2,000 statements
    of the subset (`fuzz/make_sql_corpus.sh`); its reach is the edges of
    `src/parser/parser.cpp`, against a floor in the workflow (800; the
    first run reached 894 of 3,894).

- **L27 (2026-11-20, Stage 10, decided with an advisor).** Chunk capacity.
  A `DataChunk` holds `VECTOR_SIZE` (2048) rows unless its creator sizes
  it, and the operator contract is that `GetData` gives at most that many.
  The audit below found every one of the 88 `DataChunk::Initialize` calls in
  `src/` (85 at the default capacity, 3 sized) bounded: filled by
  `DataTable::Scan` or a child's `GetData`, by an emit loop that stops at
  `VECTOR_SIZE`, row for row from one input chunk, with one row, or in
  batches of at most `VECTOR_SIZE`; the one larger chunk is
  `PhysicalInsert`'s, sized by its `VALUES` list since #45. None was seen to
  fail: `test/unit/execution/test_chunks.cpp` drives each reachable one
  with 2047, 2048, 2049, 4095, 4096 and 4097 rows and NULLs on each side of
  each boundary, against the same query over a `UNION ALL` of two halves
  (row count, an order-independent checksum, the NULL count) and the count
  the query must give, under ASan and UBSan in CI, and a coverage build
  confirmed which sites those queries reach.
  - A guard, since only a check keeps a later operator from breaking the
    contract silently. A `Vector` records its capacity (0 for one wrapping
    data it does not own); `Vector::SetValue` past it and
    `DataChunk::SetCardinality` past any column's are an
    `InternalException`. The fast paths that write `GetData<T>()[i]` are
    not checked before the write, only when the chunk is sized after it,
    and by ASan in CI. `ValidityMask` writes are not checked: they are in
    the per-row loops and the mask knows its size only in words.
  - What the boundary tests found was not a capacity bug but wrong answers
    at any size, fixed or refused here as the invariants ask: a NULL key
    grouped and partitioned with 0 or `''` (#47); keys joined with `|`, so
    rows that differ were one group, one `DISTINCT` row or one partition,
    set operations that kept rows repeated on their left, and a `PARTITION
    BY` expression ignored (#48), now one key encoding
    (`include/slothdb/execution/group_key.hpp`); `MERGE` doing other than
    it was asked (#49), now refused outside the shape it does right
    (`docs/dialect.md`). The Parquet fast paths give no rows or crash on a
    file `ParquetWriter` wrote (#50); no SQL reaches them and Stage 11
    removes them, so the Parquet run of `test_chunks.cpp` leaves those
    cases out and names the issue.
  - The audit. "Scan" is a loop over `DataTable::Scan`, at most
    `VECTOR_SIZE` rows a call; "child" a loop over a child's `GetData`;
    "emit" a loop that stops at `VECTOR_SIZE`; "row for row" at most the
    rows of one input chunk; "one row" a chunk of one row for an
    expression; "unreachable" a site no SQL reaches, with what refuses it,
    each removed in Stage 11 with the readers. Cases are those of
    `test_chunks.cpp`.

    | Site | Filled | Driven by |
    | --- | --- | --- |
    | `main/connection.cpp` `BulkLoadRows` | batches of `VECTOR_SIZE` from rows | every derived table, CTE and set operation (`derived`, `cte`, `union`) |
    | `connection.cpp` `ALTER TABLE ADD COLUMN`, two chunks | scan; the new chunk row for row | `ALTER TABLE` |
    | `connection.cpp` `ALTER TABLE DROP COLUMN`, two chunks | scan; row for row | `ALTER TABLE` |
    | `connection.cpp` `ALTER TABLE RENAME COLUMN` | scan | `ALTER TABLE` |
    | `connection.cpp` `INSERT ... SELECT` `build_coerced_chunk` | row for row from a child chunk | the four `INSERT ... SELECT` subcases |
    | `connection.cpp` `COPY TO`, three chunks (Parquet, JSON, CSV) | scan | unreachable: `COPY` is refused by the parser (`RefuseFileAccess`) |
    | `connection.cpp` `MERGE`, source and target | scan | `MERGE` |
    | `connection.cpp` `MERGE` rebuild | now `BulkLoadRows` (one row each before) | `MERGE` |
    | `connection.cpp` live view incremental CSV read | `FastCSVReader::ReadChunk`, at most `VECTOR_SIZE` | unreachable: `CREATE LIVE VIEW` refused |
    | `connection.cpp` `GENERATE_SERIES` | one row | row generators |
    | `connection.cpp` recursive CTE `make_table` | batches of `VECTOR_SIZE` | row generators (recursive CTE) |
    | `connection.cpp` `Connection::Scan` | scan | the C++ scan |
    | `catalog/table_catalog_entry.cpp` `CheckPrimaryKeyForChunk` | scan | `VALUES` into a table with a primary key |
    | `storage/data_table.cpp` `ParallelScan` | one morsel, at most `VECTOR_SIZE` | `ParallelExecutor` only, which no query reaches (`test_threads.cpp`) |
    | `storage/checkpoint.cpp` `Save` | scan | a database file reads back |
    | `checkpoint.cpp` `ReadTable` | sized by its batch, at most `VECTOR_SIZE` | a database file reads back |
    | `execution/parallel_executor.cpp`, filter and projection | row for row from a morsel | as `ParallelScan` |
    | `execution/count_group_agg.cpp` | child | `group-two`, `distinct-two` (COUNT only) |
    | `execution/literal_emit_filter.cpp`, `Init` and `GetData` | child; emit | `literal-emit` |
    | `optimizer/optimizer.cpp` `EvaluateConstant` | no column, one row | constant folding (any test) |
    | `physical_planner.cpp` `PhysicalTableScan` | scan | every case |
    | `PhysicalFilter`, input and result | child; row for row | `filter-*` |
    | `PhysicalProjection`, input and result | child; row for row | `projection` and most cases |
    | `PhysicalOrderBy` emit | emit | `order*` |
    | `PhysicalOrderBy` top-k, typed key and generic | child | `topk-*`, `group-order-limit` |
    | `PhysicalOrderBy::CollectFull` | child | `order*` |
    | `PhysicalLimit`, input and result | child; row for row | `limit*`, `topk-offset` |
    | `PhysicalInsert` | sized by the `VALUES` list (#45) | `INSERT ... VALUES` |
    | `PhysicalWindow` fallback emit and vectorised emit | emit | `window-*`, `qualify` |
    | `PhysicalWindow` per-row expression, partition expression (two), fallback rows | one row | `window-expression`, `window-partition-expression`, `window-two-windows-expression` |
    | `PhysicalWindow::ReadInput` | child | `window-*` |
    | `PhysicalDistinct`, input and result | child; row for row | `distinct-*` |
    | `PhysicalHashAggregate` emit | emit | `group-*` |
    | `PhysicalHashAggregate::ComputeAggregates` chunk | child | `group-*`, `aggregate*` |
    | `PhysicalHashAggregate`, per-row expression arguments (three) | one row | `aggregate-case*` |
    | `PhysicalHashAggregate` Parquet multi-column path | sized, batches of `VECTOR_SIZE` | Parquet only; its cases are left out (#50) |
    | `PhysicalHashAggregate` CSV workers (three) and the fused join-aggregate CSV worker | `FastCSVReader::ReadChunk` | unreachable: they need a CSV file scan, refused |
    | `PhysicalFileScan`, `PhysicalCountScan` | `ReadChunk`; one row | unreachable: CSV table functions refused |
    | `PhysicalParquetScan` | slices of a row group, at most `VECTOR_SIZE` | the Parquet table case (row groups of the file, and of 2047 rows) |
    | `PhysicalHashJoin` cross emit and streaming result (two) | emit; the probe loop checks `out < VECTOR_SIZE` per row | `join-*` |
    | `PhysicalHashJoin` probe chunk | child | `join-*` |
    | `PhysicalHashJoin` cross join collect (two) | child | `join-cross` |
    | `PhysicalHashJoin` build, left first and right after (two) | child | `join-*`; `join-left-large*` (a left side of more than 100,000 rows) |
    | `PhysicalHashJoin` build by file sizes | child | unreachable: both sides CSV file scans |
    | `PhysicalHashJoin::ResidualPasses` | one row | `join-two-keys` |
    | `PhysicalUpdate`, scan, filter, assignment, insert | scan; one row each | `UPDATE` |
    | `PhysicalDelete`, scan, filter, keep | scan; one row each | `DELETE` |
    | `PhysicalDummyScan` | no column | `scalar-subquery` and any `SELECT` without `FROM` |
    | `storage/fast_csv_reader.cpp`, `csv_reader.cpp`, `json_reader.cpp` (five) | `ReadChunk` and the JSON parse stop at `VECTOR_SIZE` | the C++ API only; no doctest crosses 2048 rows, recorded as bounded by the loop (Stage 11 removes them) |
    | `storage/sqlite_scanner.cpp`, `avro_reader.cpp`, `arrow_ipc.cpp` (six) | flush at `VECTOR_SIZE` | unreachable: refused in their constructors |

- **L28 (2026-11-20, Stage 11, decided with an advisor).** The cut of the
  file readers, and the answer to LQ2.
  - The sizing (LQ2), at `ce2a75d` (v0.3.0). zstd (`third_party/zstd`,
    39 files, 23,958 lines), `snappy` (4,793) and `fastpforlib` (2,621)
    were used by `src/storage/parquet.cpp` alone, and miniz
    (`src/storage/miniz.{c,h}`, 9,255) by `parquet.cpp` and
    `excel_reader.cpp`. The database file needs none of them:
    `Checkpoint::Save` and `Load` write and read uncompressed columns with
    a CRC-32, and include nothing from `storage/compression/`. The readers
    were 8,319 lines (Parquet 3,692, CSV 1,386, JSON 1,355, SQLite, Excel,
    Avro and Arrow IPC 1,886), fed in `physical_planner.cpp` by six scan
    operators and the Parquet and CSV fast paths of `ORDER BY ... LIMIT`,
    the aggregates, `GROUP BY`, `COUNT(DISTINCT)` and the hash join, and
    in `connection.cpp` by the handlers of `COPY`, the file table
    functions, a path used as a table, `DESCRIBE` of a file and
    `CREATE LIVE VIEW`, unreached since Stage 2 (L10).
  - Removed with the readers: each of those, the catalog's file-scan and
    live-view fields (`IsFileScan`, `SetParquetPath`, the cached
    `ParquetReader`, `MarkLiveView`, ...), `CopyStatement` and
    `CreateViewStatement::is_live`, `file_glob` and `hive_partition`,
    and the execution helpers only the fast paths called
    (`dict_histogram`, `radix_count_agg`, `int_group_agg`,
    `topk_varchar`, `string_dedup`, `radix_multi_agg`, `inline_row_agg`,
    `str_group_distinct`, `substring_search`, `simple_i64_count_map`,
    `topn_filter_pass.hpp`; 4,278 lines). A helper any remaining path uses
    stays as it is. `src/storage/compression/` goes too: SlothDB's column
    compression, which no part of the engine called, only its doctest.
    `parser.cpp` is not touched: each `RefuseFileAccess` stays as Stage 2
    wrote it, so every route is refused as before, now with nothing
    behind it.
  - `WorkerSite::PIPELINE`, the Parquet scan's decode workers, goes; no
    query reaches `ParallelFor` any more, and its doctest calls it at its
    API, as the thread pool's does.
  - Tests. The SQL refusal cases of the deleted reader tests
    (`test_csv.cpp`, `test_json_fileio.cpp`, `test_sqlite.cpp`,
    `test_arrow.cpp`, `test_parquet.cpp`) move into `test_refused.cpp`
    with their names and assertions; the Parquet files they give SQL are
    the format's magic bytes, not a file the removed writer made. The
    thread, worker-error and chunk cases driven through the Parquet scan
    go with it, not re-pointed at a table, since they tested the decode
    pipeline; the in-memory cases stay. The fixtures `groups.parquet`,
    `damaged_zstd.parquet`, `simple.sqlite` and
    `tools/make_parquet_fixtures.py` go.
  - A second golden file, `test/fixtures/written_by_v0.3.0.db`, written
    once by the shell built at `v0.3.0` (`tools/make_db_fixtures.sh
    v0.3.0`) and never regenerated, is read back against its rows and
    joins the bit-flip and truncation loop. The format is unchanged
    (1001): it is byte for byte `golden_v1001.db`, as is a file the
    shell of this stage writes.
  - `tools/check_symbols.sh` runs `nm` on `libslothdb.a`, not demangled,
    and fails on a symbol of zstd, snappy, fastpforlib or miniz
    (`ZSTD_`, `HUF_`, `FSE_`, `snappy`, `mz_`, `tinfl_`, `tdefl_`,
    `FastPForLib`, `fastunpack`, `fastpack`), on a library with no engine
    symbol, and on any of the removed paths coming back. It runs in each
    leg of `test` and in `mingw`, beside `check_dialect.sh`.
  - Left for a later clean-up, not this stage, as they change no result:
    the column mask `PlanProjection` hands to an `ORDER BY` below it
    (`SetParentProjectionCols`), which only the Parquet top-k read, and
    comments in `physical_planner.cpp` that still name the Parquet
    paths.

- **L29 (2026-11-20, Stage 11.1, decided with an advisor).** The
  provider interface of L25, and where the engine runs it.
  - Worker threads. After Stage 11, one query path runs on worker
    threads: the window operator's per-partition sort, from 200,000 input
    rows and two partitions at a thread cap of 2 or more
    (`PhysicalWindow`, a `WorkerGroup`). It compares precomputed keys and
    column values and evaluates no expression, so it calls no provider.
    `ParallelExecutor` and `ParallelFor` are reached by no query. So the
    engine calls every provider on the query's thread, until a parallel
    operator that evaluates expressions returns. The contract on
    providers stays L25's (safe from any thread, on distinct match
    states), and match state is made per `Execute` call, so such an
    operator would need no change. The thread doctests therefore drive
    each provider path from 1, 2 and 4 threads directly, one compiled
    pattern shared and match state per thread, which the `linux-tsan` job
    runs; one more runs the parallel window sort over a regex key at caps
    1, 2 and 4. The SQL fuzz target's oracle at caps 1 and 2 is unchanged:
    it runs with no provider, so the regex forms are refused at both caps,
    and the caps differ only where the window sort runs.
  - The tables are the engine's own: `include/slothdb/main/providers.hpp`,
    plain C, `zugodb_regex_v1` and `zugodb_utf8_v1`, each headed
    `{uint32_t abi_version; uint32_t struct_size;}`, with the engine's
    status codes, in a `zugodb_providers` that also holds reserved slots
    for UUIDs, random numbers and decimals. The family's tables already
    differ in their headers (zuutf's has no ABI field; zuregex's is
    PCRE2-shaped and changes with zuregex#211), so zugodb's glue adapts
    them with small functions rather than the engine mirroring either.
  - `Database::SetProviders`, once and before the first query; a second
    call, or one after a query, throws. A set that is NULL, of another
    ABI version or too small throws (the host's bug). A service table that
    is NULL, of another version, too small or has a NULL member is absent:
    its functions are refused as with no table, and `GetProviderState`
    says which. A larger `struct_size` is accepted. The members are copied
    into the `Database`, and carried to the executor on each
    `BoundFunction`, never in a static, so two databases never cross.
    Reserved slots are not read.
  - Regex. Through the table: `regexp_matches`, `regexp_full_match`,
    `regexp_replace` and `regexp_extract` with a constant integer group,
    and `[NOT] SIMILAR TO` as `regexp_full_match`. Their options are a
    constant of `c` and `i` (the last wins), and `g` for
    `regexp_replace`; another letter is refused. Still refused with a
    table: `~` and `!~` (in the tokenizer), the functions that return a
    `LIST` (`regexp_extract_all`, `regexp_split_to_array`,
    `string_split_regex`, ...), `regexp_split_to_table`, `regexp_escape`,
    `regexp`, `regexp_like`, `regexp_match` and `SIMILAR TO ... ESCAPE`.
    Without a usable table every refusal stays where L24 put it, with its
    message: `Parser::Parse` takes `ParseOptions::regex` from the
    `Connection`, so `SIMILAR TO` is still refused by the parser. The
    rewrite (`\0` to `\9`) and the global-replace loop are the
    provider's, as zuregex#211 puts them in `zr_substitute`; a short
    output buffer is retried once at the size the provider asks for.
  - A constant pattern is compiled at bind, once per query, and an
    invalid one is a bind error; a pattern computed per row is compiled
    when its text changes. Match state is made per `Execute` call (one
    chunk, one thread) and freed by its destructor, on a throw too. Every
    status but OK and no-match is a query error with the provider's text;
    a limit hit says so and is never taken as no match.
  - The engine cannot check that a table has DuckDB's RE2 semantics. A
    host gives no regex table until its library has that dialect
    (zuregex#211): a PCRE-backed one would answer the same SQL otherwise.
    An invalid UTF-8 subject is the provider's error, so a query error.
  - UTF-8. `LENGTH`'s count of non-ASCII text goes through the table:
    `validate`, then `count` of valid text; invalid text keeps the
    engine's count, so the answer is the same with or without the table,
    and another status is an error. The positions (`CodePointStarts`,
    `CodePointPosition`) stay the engine's: the table gives no offsets.
    No invalid UTF-8 is refused yet; refusing it must hold with and
    without a provider, with a built-in validator at `Append`, literals
    and `Checkpoint::Load`, and is a change of its own.
  - Tests: stub tables in `test/unit/stub_providers.hpp` (a literal
    matcher, and a UTF-8 counter), no package of the family built here.

- **L30 (2026-11-20, #42, decided with an advisor).** NaN with DuckDB's
  semantics, superseding L23's refusal.
  - The order. A NaN equals every NaN, whatever its sign or payload, and
    is greater than every other value, `+inf` included; `-0.0` equals
    `0.0`. One header, `include/slothdb/common/types/float_order.hpp`,
    holds it (`SqlEq`, `SqlLess`, templates that are the built-in
    operators for any other type), and `Value::operator==` and `<` use
    it, so every path that compares `Value`s (sorts, set operations,
    `IN`, `NULLIF`, `GREATEST`, window peers, `QUALIFY`) has it. NaN is
    told by its bits, not `std::isnan`, which `-ffast-math` folds away
    in the R package's build. A hash or a group key canonicalises
    (`CanonicalKey`: every NaN one bit pattern, `-0.0` as `0.0`); the
    value a group, a `DISTINCT` row or a join shows keeps its own bits.
  - The audit. Every raw `double` or `float` comparison found in
    `physical_planner.cpp`, `count_group_agg.cpp`, `agg_emit_helpers.cpp`
    and `connection.cpp` uses the header: the typed `ORDER BY ... LIMIT`
    heap, the window's typed sorts (serial and the parallel one), peers,
    `ROW_NUMBER() = 1` reduction and `MIN`/`MAX`, the hash aggregate's
    `MIN`/`MAX`, `COUNT(DISTINCT)` and single-column key, `MEDIAN`'s sort,
    the hash join's keys (a FLOAT or DOUBLE key on either side is the
    value as a canonical double; its text kept `nan` from `-nan` and
    `-0.0` from `0.0`), and the set operations' row hash. Two fast paths
    no longer take floating-point values and leave them to the generic
    path: the COUNT-only multi-column `GROUP BY` (its key's raw bytes) and
    the fused join-aggregate, which also read a probe-side `FLOAT`,
    `SMALLINT` or `TINYINT` as 0. The simple-predicate compiler and the
    GPU engine, which no query reaches, are left IEEE's.
  - Ties. `MIN` and `MAX` keep the last of equal values, as DuckDB's
    (`min` over `0.0` then `-0.0` is `-0.0`); `GREATEST` and `LEAST` the
    first, except that DuckDB's of two or more `FLOAT`s returns through
    its sort key, so `-0.0` is `0.0` and a NaN the positive one there.
  - Making and holding one. `0/0`, `inf - inf`, `sqrt(-1)`, `ln(-1)`,
    `acos(2)`, `x % 0` of doubles give the hardware's NaN, as DuckDB's
    default `ieee_floating_point_ops` does; the trigonometric functions
    gave NULL for a NaN or an infinity and `sign(NaN)` NULL, now DuckDB's
    NaN, infinity and 0. `%` and `mod` of operands DuckDB types `DECIMAL`
    or integer (`10.0 % 0.0`) stay "Division by zero" (the binder's
    `decimal_mod`), and `-0.0` as a literal is DuckDB's `DECIMAL` zero,
    `0.0`. Text is read by DuckDB's rules (`nan`, `-nan`, `nan(...)`,
    `inf`, `infinity`, spaces and line ends around; no hexadecimal; past
    the range an infinity, below it 0); a NaN cast to an integer is out
    of range, to `BOOLEAN` true. `Append`, `Checkpoint::Load` (any bits:
    the format is unchanged, 1001, and a file with a NaN that 0.3.0
    refused now loads) and results hold a NaN with its bits.
  - Text. A `FLOAT` or `DOUBLE` cast to `VARCHAR` (and `||`, `concat`,
    `string_agg`) is DuckDB's: the shortest digits, `.0` on a whole
    number, an exponent outside `[1e-4, 1e16)`, `-0.0`, `inf`, and `nan`
    or `-nan` by the sign bit. It was `1` for `1.0`, and a NaN as the C
    library prints it (`nan(ind)` under MinGW). The sign of a computed
    NaN is the hardware's (`-nan` from `0/0` on x86-64, `nan` on arm64),
    as DuckDB's is; a literal's and its negation's are fixed.
  - Types the new text exposed, made DuckDB's: `ROUND` and `TRUNC` of an
    integer keep its type (with DuckDB's rounding to a power of ten and
    "Overflow in ROUND of integer"), `MOD` of integers is `%`, and
    `FLOAT` arithmetic with a `FLOAT` or an integer is `FLOAT` (it was
    `DOUBLE`, digits DuckDB does not give). `CEIL` and `FLOOR` of an
    integer are `DOUBLE`s in DuckDB too.
  - Found on the way and fixed, as the comparisons go through them:
    `BETWEEN` and `IN` of mixed types (a `BIGINT` or `DOUBLE` column
    `BETWEEN 1 AND 10` was true of every row, `IN (2, 3)` of none, a
    `DATE BETWEEN` two strings always true) now unify their arguments'
    types (`UnifyTypes`, a string literal taking the other's type;
    `VARCHAR` column `IN (1)` is refused), and `BETWEEN` with a `NULL`
    bound is three-valued. `INSERT ... SELECT` of a `DOUBLE` into an
    integer column rounds half to even as DuckDB's cast (it truncated,
    and a NaN's cast was undefined). Refused rather than fixed: a
    `VALUES` column mixing string literals with another type (#59), and
    text with a fraction or an exponent cast to an integer (DuckDB
    rounds `'1.5'` to 2; `TRY_CAST` gave NULL).
  - The oracle. `tools/make_nan_cases.py` runs 1,158 queries over NaN of
    both signs, the infinities, both zeros, finite values and NULL
    (comparisons, filters, `ORDER BY` both ways with `NULLS FIRST`/`LAST`
    and `LIMIT`, `GROUP BY`, `DISTINCT`, `COUNT(DISTINCT)`, the
    aggregates, joins inner and outer, set operations, windows, casts,
    arithmetic and functions, and a 200,000-row table past the fast
    paths' thresholds) against DuckDB v2.0.0's Python package and writes
    `test/fixtures/nan/duckdb_cases.tsv`. `test/unit/execution/test_nan.cpp`
    replays it at thread caps 1 and 2 and over the tables saved and
    loaded: a NaN matches any NaN, any other double bit for bit (`-0.0`
    is not `0.0`) except where the case marks the zero's sign as DuckDB's
    choice among equal values, and an error must match an error. The four
    cases the engine refuses are listed with their reasons in
    `test/fixtures/nan/refused.tsv`, and one that starts passing fails
    the test. The SQL fuzzer's tables hold NaN, `-NaN`, the infinities
    and `-0.0`.

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
- **LQ2 (settled 2026-11-20, Stage 11).** When the engine's readers are
  cut, what else in `src/storage/` still needs zstd, snappy or miniz?
  Nothing: only `parquet.cpp` and `excel_reader.cpp` used them, and the
  database file's own format compresses nothing. The sizing and the cut
  are L28.

[zdesign]: https://github.com/pedrobtz/zugodb/blob/main/.agents/design.md
