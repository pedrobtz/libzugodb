# libzugodb: roadmap

Status: draft 2026-11-19. Implements [design.md](design.md) and the engine
half of [zugodb's roadmap][zroadmap], Stages 6, 8 and 10.
Target: three releases, each vendored by zugodb when tagged.
Tracking: [v0.2.0 (#2)](https://github.com/pedrobtz/libzugodb/issues/2),
[v0.3.0 (#9)](https://github.com/pedrobtz/libzugodb/issues/9) and
[v0.4.0 (#15)](https://github.com/pedrobtz/libzugodb/issues/15), with one
sub-issue per stage.

This is a C++ library, not an R package, so there is no CRAN-submission
issue, no board card and no r-actions profile here: zugodb has those. The
closing stages of an R roadmap (Assurance, Full CI, readiness, Review) are
zugodb's Stages 12 to 15, and they run over the engine as vendored.

**CI.** Every pull request runs `.github/workflows/ci.yml`: the doctest
suite on Linux gcc, Linux with ASan and UBSan, macOS clang and MinGW-w64.
The CI column names the job a stage adds; `ci` means none. A job a stage
adds is required from that stage on.

**Releases.** A version is released when its last stage merges: the
`CHANGELOG.md` entry, the tag, a GitHub release, and the version's parent
issue closed. zugodb then moves its pin.

## Stages

<!-- markdownlint-disable MD013 -->
| Stage | Title | Version | CI | Issue | Pull request | Done |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | [Repository clean-up](#stage-0--repository-clean-up) | 0.2.0 | ci | [#8](https://github.com/pedrobtz/libzugodb/issues/8) | [#19](https://github.com/pedrobtz/libzugodb/pull/19) | 2026-11-19 |
| 1 | [Errors on worker threads become query errors](#stage-1--errors-on-worker-threads-become-query-errors) | 0.2.0 | ci | [#3](https://github.com/pedrobtz/libzugodb/issues/3) | [#20](https://github.com/pedrobtz/libzugodb/pull/20) | 2026-11-20 |
| 2 | [Refuse the CSV, Parquet and JSON table functions](#stage-2--refuse-the-csv-parquet-and-json-table-functions) | 0.2.0 | ci | [#4](https://github.com/pedrobtz/libzugodb/issues/4) | [#21](https://github.com/pedrobtz/libzugodb/pull/21) | 2026-11-20 |
| 3 | [A checksum for the database file, and format version 1001](#stage-3--a-checksum-for-the-database-file-and-format-version-1001) | 0.2.0 | ci | [#5](https://github.com/pedrobtz/libzugodb/issues/5) | [#22](https://github.com/pedrobtz/libzugodb/pull/22) | 2026-11-20 |
| 4 | [Harden Checkpoint::Load, with a fuzz harness in CI](#stage-4--harden-checkpointload-with-a-fuzz-harness-in-ci) | 0.2.0 | + `fuzz` | [#6](https://github.com/pedrobtz/libzugodb/issues/6) | [#24](https://github.com/pedrobtz/libzugodb/pull/24) | 2026-11-20 |
| 5 | [ThreadSanitizer in CI](#stage-5--threadsanitizer-in-ci) | 0.2.0 | + `linux-tsan` | [#7](https://github.com/pedrobtz/libzugodb/issues/7) | [#27](https://github.com/pedrobtz/libzugodb/pull/27) | 2026-11-20 |
| 6 | [Conformance runner and first counts](#stage-6--conformance-runner-and-first-counts) | 0.3.0 | + `conformance` | [#10](https://github.com/pedrobtz/libzugodb/issues/10) | [#28](https://github.com/pedrobtz/libzugodb/pull/28) | 2026-11-20 |
| 7 | [No crash and no hang on the pinned subset](#stage-7--no-crash-and-no-hang-on-the-pinned-subset) | 0.3.0 | ci | [#11](https://github.com/pedrobtz/libzugodb/issues/11) | [#29](https://github.com/pedrobtz/libzugodb/pull/29) | 2026-11-20 |
| 8 | [No wrong answer on the pinned subset](#stage-8--no-wrong-answer-on-the-pinned-subset) | 0.3.0 | ci | [#12](https://github.com/pedrobtz/libzugodb/issues/12) | [#30](https://github.com/pedrobtz/libzugodb/pull/30), [#41](https://github.com/pedrobtz/libzugodb/pull/41) | 2026-11-20 |
| 9 | [Parser hardening and the dialect reference](#stage-9--parser-hardening-and-the-dialect-reference) | 0.3.0 | `fuzz` gains a target | [#13](https://github.com/pedrobtz/libzugodb/issues/13) | [#46](https://github.com/pedrobtz/libzugodb/pull/46) | 2026-11-20 |
| 10 | [Chunk capacity audit](#stage-10--chunk-capacity-audit) | 0.3.0 | ci | [#14](https://github.com/pedrobtz/libzugodb/issues/14) | [#52](https://github.com/pedrobtz/libzugodb/pull/52) | 2026-11-20 |
| 11 | [Cut the engine's file readers](#stage-11--cut-the-engines-file-readers) | 0.4.0 | ci | [#16](https://github.com/pedrobtz/libzugodb/issues/16) | [#54](https://github.com/pedrobtz/libzugodb/pull/54) | 2026-11-20 |
| 11.1 | [Host-supplied function providers](#stage-111--host-supplied-function-providers) | 0.4.0 | ci | [#40](https://github.com/pedrobtz/libzugodb/issues/40) | | |
| 12 | [The names become zugodb](#stage-12--the-names-become-zugodb) | 0.4.0 | ci | [#17](https://github.com/pedrobtz/libzugodb/issues/17) | | |
<!-- markdownlint-enable MD013 -->

## Stage 0 — Repository clean-up

**Issue.** [#8](https://github.com/pedrobtz/libzugodb/issues/8), sub-issue
of [v0.2.0](https://github.com/pedrobtz/libzugodb/issues/2).

**CI.** `ci`.

**Goal.** The repository holds the engine and nothing else, and what
zugodb vendors is unchanged, byte for byte.

**Work.**

- Delete what design §6 lists as removed, the `llama.cpp` submodule and
  `.gitmodules` included.
- `CMakeLists.txt` and `src/CMakeLists.txt` lose `SLOTHDB_ASK_MODEL`,
  `SLOTHDB_EDGE` and `SLOTHDB_BUILD_SHARED`.
- `src/api/`, `include/slothdb/api/` and any test of the C API go
  (design L5).
- `bench/correctness/sqllogictest/` stays until Stage 6 replaces it.
- `CONTRIBUTING.md` rewritten for this repository; `README.md` and
  `CLAUDE.md` brought in line; `.github/codeql` kept only if a workflow
  uses it.
- A `CHANGELOG.md` entry under 0.2.0 that names the removed C API.

**Tests.**

- The vendored paths are unchanged: `git diff v0.1.1 -- LICENSE include
  src third_party/zstd third_party/fastpforlib third_party/snappy` shows
  only `src/CMakeLists.txt`, `src/api/` and `include/slothdb/api/`, which
  zugodb excludes. Shown in the pull request.
- Nothing that remains refers to something removed: a grep for each
  removed path and option over the tree, shown in the pull request.
- The doctest suite passes with the same case count as before, less the
  C API's cases, on all four CI jobs.

**Exit criteria.**

- Every `**Tests.**` line holds.
- A fresh clone has no submodule and builds with the README's three
  commands.

**Deviations (2026-11-20).**

- The shell in `tools/shell/` was built on the C API, which the stage did
  not say. It now uses `Database` and `Connection`.
- The two `**Tests.**` lines conflict for `src/` and `include/`: the
  `#ifdef SLOTHDB_EDGE` blocks in `src/main/connection.cpp` and
  `src/execution/physical_planner.cpp`, and two comments that mention the
  C API, refer to something removed, and removing them would change what
  zugodb vendors. The vendored paths win; the grep excludes those lines,
  and the pull request lists them. Design L8.

**Design.** [§6](design.md#6-what-the-repository-holds) and L5, L6 in
[§8](design.md#8-decisions).

## Stage 1 — Errors on worker threads become query errors

**Issue.** [#3](https://github.com/pedrobtz/libzugodb/issues/3), sub-issue
of [v0.2.0](https://github.com/pedrobtz/libzugodb/issues/2).

**CI.** `ci`.

**Goal.** An exception on an engine worker thread is an error of the
query, not `std::terminate`.

**Work.**

- Each worker catches, keeps the first exception, and the query's thread
  rethrows it after the join. Every `std::thread` and `ThreadPool` task in
  `src/` is covered, not only Parquet's: `physical_planner.cpp` starts raw
  threads at a dozen or more sites.
- A test seam that makes a worker throw at a chosen site.

**Tests.**

- One doctest per kind of site, with a worker made to throw: the
  exception arrives as a query error, and the same connection then
  answers another query. In `test/unit/execution/test_worker_errors.cpp`.
- The damaged Parquet file of SouravRoy-ETL/slothdb#33: an error, not an
  abort, in the same file.
- Every thread start in `src/` is one of the covered kinds: a grep for
  `std::thread` and the pool's submit call, listed in the pull request.

**Exit criteria.**

- Every `**Tests.**` line exists and was seen to fail.
- The ASan and UBSan job is green: no leak on the error path.

**Deviations (2026-11-20).**

- No query reaches `ThreadPool`: its one user is
  `ParallelExecutor::ParallelScanFilter`, which only tests call. Its
  doctest checks the error at that API, then that the connection answers.
- The thread-start grep is also a doctest, in the same file, so a later
  stage that adds an unwrapped thread fails the suite.
- The Parquet cases read committed fixtures written by DuckDB
  (`tools/make_parquet_fixtures.py`, design L9), not files written by the
  engine's `ParquetWriter`: a `GROUP BY` over such a file returns no rows,
  a wrong answer that this stage does not fix. They reach the reader
  through `read_parquet()`, which Stage 2 refuses; Stage 2 moves them to
  the reader's C++ API or an internal path.

**Design.** [zugodb Stage 6][zroadmap] and the assessment's A1.

## Stage 2 — Refuse the CSV, Parquet and JSON table functions

**Issue.** [#4](https://github.com/pedrobtz/libzugodb/issues/4), sub-issue
of [v0.2.0](https://github.com/pedrobtz/libzugodb/issues/2).

**CI.** `ci`.

**Goal.** No SQL reaches one of the engine's file readers.

**Work.**

- `read_csv`, `read_csv_auto`, `read_parquet`, `read_json`, `read_ndjson`,
  a file path used as a table, `DESCRIBE 'file'` and `COPY` to or from a
  file are refused, as Arrow, SQLite, Excel and Avro already are. The
  message is engine-neutral: reading files from SQL is not supported.
- Doctests that read files become checks of the refusal. The readers'
  code stays until Stage 11.

**Tests.**

- Each function, and each extension as a bare path, is refused before a
  byte is read: the path given does not exist, and the error is the
  refusal, not "file not found". In
  `test/unit/storage/test_refused.cpp`.
- The connection answers a query after each refusal, in the same file.

**Exit criteria.**

- Every `**Tests.**` line exists and was seen to fail.
- No doctest opens a data file through SQL: a grep, shown in the pull
  request.

**Deviations (2026-11-20).**

- `CREATE LIVE VIEW` is refused too: it watched a file and read it again
  when it changed. Its seven doctests become two refusal checks.
- `test/unit/execution/test_httpfs.cpp` goes: three of its four cases ran
  only with a network and an environment variable, the fourth checked that
  a local path was not fetched, and remote paths now meet the refusal
  first; `test_refused.cpp` checks them.
- The CSV and JSON scan paths in `physical_planner.cpp`, reached only by a
  file named in SQL, are no longer tested; the readers are, through their
  C++ API. The Parquet scan operator stays tested through a table set up
  with the catalog (design L10), since Stage 1's worker cases need it.

**Design.** [design §2](design.md#2-scope) and
[zugodb §7.5][zdesign] (Q17).

## Stage 3 — A checksum for the database file, and format version 1001

**Issue.** [#5](https://github.com/pedrobtz/libzugodb/issues/5), sub-issue
of [v0.2.0](https://github.com/pedrobtz/libzugodb/issues/2).

**CI.** `ci`.

**Goal.** A damaged database file, or one of another version, does not
open.

**Work.**

- The writer stores one checksum over the file (CRC-32 or similar, no new
  dependency), and the format version becomes 1001, libzugodb's own range.
- Opening a file whose checksum fails, or of any other version, is an
  error naming the cause and both versions.
- A golden database file under `test/fixtures/`, with the script that
  made it.

**Tests.**

- The golden file read back against its recorded rows, in
  `test/unit/storage/test_persistence.cpp`.
- One bit flipped at each of a spread of offsets from the header to the
  last byte, and the file truncated at each: none opens, and a new
  database then works. Same file.
- An upstream-format file (version 1) is refused with a message naming 1
  and 1001. Same file.
- The checksum itself: the standard check value of the algorithm chosen
  (for CRC-32, `0xCBF43926` over `"123456789"`), in
  `test/unit/common/`.

**Exit criteria.**

- Every `**Tests.**` line exists and was seen to fail.
- The golden file is read on all four CI jobs, so byte order and MinGW
  are covered.

**Design.** [zugodb §8][zdesign] and Q7.

## Stage 4 — Harden Checkpoint::Load, with a fuzz harness in CI

**Issue.** [#6](https://github.com/pedrobtz/libzugodb/issues/6), sub-issue
of [v0.2.0](https://github.com/pedrobtz/libzugodb/issues/2).

**CI.** `+ fuzz`: a libFuzzer job that runs each harness for a fixed time
on pull requests, and longer on a schedule.

**Goal.** A database file made to pass the checksum still cannot crash,
hang or over-allocate the loader.

**Work.**

- Every length, count and offset `Checkpoint::Load` reads is bounded
  against the bytes left.
- `fuzz/checkpoint_load.cpp`: the harness recomputes the checksum of each
  input, so the fuzzer reaches the loader. A corpus of valid files, and
  the script that makes it.
- The `fuzz` job, with a floor on the loader's lines reached.

**Tests.**

- The harness's oracle: a file that opens gives the same rows on a second
  open, and each table's rows equal its `count(*)`. Any crash, hang or
  sanitizer report fails.
- Each bound, at the limit and one past it, as a doctest with a
  hand-built file, in `test/unit/storage/test_persistence.cpp`.
- Each crash the fuzzer finds becomes a fixture and a doctest.

**Exit criteria.**

- The `fuzz` job is green with reach at or above its floor.
- Every `**Tests.**` line exists and was seen to fail.

**Design.** [zugodb Stage 6][zroadmap] and the assessment's A8.

**Deviations (2026-11-20).**

- Reach is counted in edges, not lines: the edges of
  `src/storage/checkpoint.cpp` that the kept corpus covers, as libFuzzer's
  `-print_coverage=1` reports them (`fuzz/run.sh`). libFuzzer reports
  edges per function without another tool; the floor is set from the
  observed runs.
- Beyond the loader: `RowGroup` takes a capacity, so a load allocates for
  the rows the file holds, and the TIME, DATE and TIMESTAMP formatters in
  `value.cpp` no longer overflow, a fault the fuzzing found (design L12).
- The local fuzzing ran a random-mutation driver over the harness under
  ASan and UBSan, as Apple clang has no libFuzzer and Homebrew's llvm is
  not installed here; libFuzzer itself ran in CI.

## Stage 5 — ThreadSanitizer in CI

**Issue.** [#7](https://github.com/pedrobtz/libzugodb/issues/7), sub-issue
of [v0.2.0](https://github.com/pedrobtz/libzugodb/issues/2).

**CI.** `+ linux-tsan`.

**Goal.** A data race in the engine fails a pull request.

**Work.**

- A `linux-tsan` job: `-fsanitize=thread`, the doctest suite, and a loop
  of multi-threaded queries over in-memory tables at thread caps 2 and 4.
- Each race it finds is fixed in its own pull request first.
- Release 0.2.0.

**Tests.**

- The job reports the race of [#1](https://github.com/pedrobtz/libzugodb/pull/1)
  when its fix is reverted: shown once, in the pull request.
- Each race fixed has a doctest that TSan fails without the fix.

**Exit criteria.**

- `linux-tsan` is green with no suppression, or each suppression names an
  issue.
- `v0.2.0` is tagged and released.

**Design.** [design §5](design.md#5-conformance-and-its-oracles), threads.

**Deviations (2026-11-20).**

- The races TSan found are fixed in this stage's pull request, one commit
  each with its doctest, not in pull requests of their own: each is a
  few lines, and the job could not be green without them.
- Over an in-memory table only the window operator takes worker threads
  (it sorts partitions on workers from 200,000 rows and two partitions);
  GROUP BY, DISTINCT, ORDER BY, joins and aggregates run on the query's
  thread at any size, as the planner's other parallel paths are the
  Parquet, CSV and JSON scans'. So the loop
  (`test/unit/execution/test_threads.cpp`) also runs those shapes over a
  Parquet table set up through the catalog, and `ParallelExecutor`
  through its API, which no query reaches. Every query's rows at caps 2
  and 4 must equal those at cap 1, in every CI job.

## Stage 6 — Conformance runner and first counts

**Issue.** [#10](https://github.com/pedrobtz/libzugodb/issues/10),
sub-issue of [v0.3.0](https://github.com/pedrobtz/libzugodb/issues/9).

**CI.** `+ conformance`: runs the pinned subset and compares its counts
with the committed ones.

**Goal.** How much of the engine's SQL is right is measured on this tree,
reproducibly, and written down.

**Work.**

- `tools/slt/`: a C++ sqllogictest runner over `Connection::Query`. It
  reads DuckDB's test format as far as the subset needs it (`statement
  ok`, `statement error`, `query` with its type string and sort mode,
  `require`, `mode skip`, `loop`), and leaves a file out when it meets a
  directive it does not implement.
- A driver that runs one file per child process under a time limit, and
  classifies each case as design §5 does.
- `test/conformance/`: the pinned DuckDB tag, a fetch script that checks
  out that tag, the list of files in the subset, and the committed
  counts per file. The corpus itself is fetched, not committed.
- Every wrong, accepted, crashed and hung case is listed in a committed
  file with its SQL.
- `bench/correctness/` is removed.

**Tests.**

- The runner itself: a hand-written `.test` file with one case of each
  class, including a planted wrong value, a wrong type and a wrong row
  order under `nosort`, is classified as written. In `test/unit/`,
  through the runner's library.
- The value formatter (NULL, floats, empty string, booleans): DuckDB's
  recorded text for each, taken from the corpus.
- A file that crashes and one that loops: counted as crashed and hung,
  and the run goes on. Two fixtures.
- The subset at thread caps 1 and 2: equal results, by the driver.
- The committed counts against a fresh run: equal, by the `conformance`
  job.

**Exit criteria.**

- The counts are committed, with the tag and the file list
  (LQ1 settled in the design).
- The `conformance` job is green on Linux and macOS, and finishes in
  under ten minutes.
- Every `**Tests.**` line exists and was seen to fail.

**Deviations (2026-11-20).**

- Two counts beside §5's six (design L16): unreliable, a wrong or
  accepted case after a setup statement of its file failed, and not run,
  the cases after a crash or a hang in their file.
- The time limit is per case, 30 seconds without progress, not per file,
  so a slow file is not a hang; a child past 2 GiB of resident memory is
  killed and its case hung too (L16).
- The committed counts are those of an ASan and UBSan build: a Release
  build crashed on different cases from run to run (a use after free in
  `PhysicalOrderBy`), so its counts could not be committed (L16).
- Caps 1 and 2 are compared by each case's class, not its values: a value
  read from uninitialised memory differs from run to run (L16).
- The counts are committed per platform (`linux-x86_64`, `macos-arm64`):
  the engine gives three cases a different class on each (L16).
- A `statement ok` that only sets one of DuckDB's execution settings
  (`threads`, `debug_*`, optimizer and join thresholds, ...) is not run
  (L17).
- A `statement error`'s expected message is not compared: an error is an
  error.
- Design L9 moves the Parquet fixtures' `duckdb==` pin to the corpus's tag
  once LQ1 is settled; the fixtures are not regenerated in this stage.

**Design.** [§5](design.md#5-conformance-and-its-oracles); L1 and L3 in
[§8](design.md#8-decisions); LQ1 in [§9](design.md#9-open-questions).

## Stage 7 — No crash and no hang on the pinned subset

**Issue.** [#11](https://github.com/pedrobtz/libzugodb/issues/11),
sub-issue of [v0.3.0](https://github.com/pedrobtz/libzugodb/issues/9).

**CI.** `ci`.

**Goal.** No case of the subset ends the process or runs past the limit,
under the sanitizers as well.

**Work.**

- Each crashed and hung case from Stage 6's list is fixed, or its
  statement is refused with an error. Refusing is the default where the
  fix is not small.
- The `conformance` job also runs under ASan and UBSan.
- If the list does not fit one review, the stage lands the fixes it has
  and each remaining case is a sub-issue of
  [v0.3.0](https://github.com/pedrobtz/libzugodb/issues/9).

**Tests.**

- Each case fixed or refused: a doctest with its SQL, expecting DuckDB's
  recorded result or an error, in `test/unit/execution/`.
- The committed counts: crashed 0 and hung 0, with and without the
  sanitizers.

**Exit criteria.**

- The committed counts show no crash and no hang, and the `conformance`
  job holds them there.
- Every case from the list has a doctest that was seen to fail.

**Deviations (2026-11-20).**

- The `conformance` job already ran under ASan and UBSan from Stage 6
  (design L16), so this stage adds no job. The counts are committed for
  that build only; a Release build is not counted, as in Stage 6.
- All 21 cases are fixed; none is refused as a whole. Two fixes refuse
  what they cannot do instead: an `ORDER BY` item of a window query that
  is neither an output column nor an expression of source columns, and a
  `date_trunc` part the engine does not know (design L18).
- The fixes let the cases after each crash run; some of those are wrong
  or accepted (a window `skewness`, `percent_rank() over ()`, `CHR(-10)`,
  `REPEAT('hello', 'world')`, ...). They are Stage 8's. No case that
  passed or was refused before became wrong or accepted.

**Design.** [§5](design.md#5-conformance-and-its-oracles), L18 in
[§8](design.md#8-decisions) and [zugodb Q18][zdesign].

## Stage 8 — No wrong answer on the pinned subset

**Issue.** [#12](https://github.com/pedrobtz/libzugodb/issues/12),
sub-issue of [v0.3.0](https://github.com/pedrobtz/libzugodb/issues/9).

**CI.** `ci`.

**Goal.** Q18's bar holds for the engine: every result it gives on the
subset is DuckDB's.

**Work.**

- Each wrong case is fixed, refused, or listed as a difference with its
  reason. Each accepted case (a result where DuckDB raises an error) is
  read, and is a listed difference or a defect treated the same way.
- Cases are grouped by cause before fixing; one cause is one commit.
- The same rule for one review as Stage 7: what does not fit becomes
  sub-issues of [v0.3.0](https://github.com/pedrobtz/libzugodb/issues/9).

**Tests.**

- Each cause: a doctest with the smallest SQL that shows it, expecting
  DuckDB's recorded result or an error, in `test/unit/execution/`.
- The committed counts: wrong 0, at thread caps 1 and 2.
- The listed differences: each row's SQL still differs as described, so a
  row that stops being true fails. In `test/conformance/`.

**Exit criteria.**

- The committed counts show no wrong answer, and the `conformance` job
  holds them there.
- No accepted case is without a fix, a refusal or a listed difference.
- Every cause has a doctest that was seen to fail.

**Deviations (2026-11-20).**

- The causes did not fit one review. PR #30 lands thirteen: joins whose
  condition has no column equality, `NATURAL JOIN`, date, time and
  timestamp text (#25), the timestamp variants, `DECIMAL` (#26), window
  shapes, UTF-8 string functions, aggregates, a `BIGINT` sum past 2^53,
  small integers in floating-point arithmetic, and the runner's regex
  results. The other eight causes, 86 cases (61 wrong, 25 accepted on the
  macOS x86_64 ASan run), are sub-issues of
  [v0.3.0](https://github.com/pedrobtz/libzugodb/issues/9): #31 to #38;
  #39 is the Unicode case table that would lift a refusal of L22. PR
  #41 closes #31 to #38 and the exit criteria hold: wrong 0 and
  accepted 0 on both platforms, at thread caps 1 and 2. #39 stays open
  and is not Stage 8's (a refusal, not a wrong answer).
- PR #41 also fixed wrong answers the subset did not count, found
  while fixing those it did: `ORDER BY` of `DATE`, `TIME` and
  `TIMESTAMP` values, NULLs after `DESC` (DuckDB's `nulls_last`) and
  under `LIMIT`, `ORDER BY` of two keys under `LIMIT`, recursive CTEs,
  `/` of integers (a double division in DuckDB), `^` (`POWER`), integer
  overflow (an error; #43 tracks `TINYINT` and `SMALLINT` arithmetic,
  which still widens where DuckDB overflows), and a `GROUP BY` of a join
  key. Decided with an advisor: the engine holds no NaN, and `CREATE
  TABLE` returns DuckDB's `Count` (L23; #42 tracks DuckDB's NaN
  semantics). Decided by the maintainer, over an advisor's
  recommendation of a Pike VM, which was built and dropped: every
  regular expression is refused until a host's RE2 provider (#40)
  supplies them (L24). #32 closes by refusal.
- Cases that passed and are now refused, 69 on the macOS run: 35 that
  use a regular expression (L24; they passed under `std::regex`), 24
  that make or read a NaN (L23), 7 `ORDER BY` shapes (a constant, which
  DuckDB refuses but the corpus also runs under
  `order_by_non_integer_literal`, a setting the engine refuses; a
  set-operation key that is neither an output name nor position, which
  passed only because the rows came sorted), a `LIKE` pattern ending in its escape where DuckDB
  stops before reaching it, a `BLOB || VARCHAR`, and `mod(42, 0)` under
  `error_on_division_by_zero=false`, a setting the engine refuses. Four
  cases moved from passed or refused to unreliable, each after a setup
  statement the engine refuses: `SET ieee_floating_point_ops=false` in
  `types/null/test_null.test` (the engine now gives DuckDB's default
  answer, an infinity, where the file expects the setting's NULL), and an
  `INSERT` of NaN in `types/float/nan_functions.test`.
- `EXPLAIN` queries are left out by the runner, not listed as
  differences (L20). No listed difference was needed, in PR #30 or
  #41, so `test/conformance/differences.tsv` and its test are not
  added: the Tests line on listed differences holds vacuously.
- Many cases that passed are now refused: those that used `DECIMAL` (143
  on one run), `ILIKE` and `UPPER`/`LOWER` of non-ASCII text, and a
  correlated subquery in a join condition, which passed only because the
  condition was ignored. No case moved from passed or refused to wrong
  or accepted.

**Design.** [§3](design.md#3-dialect),
[§5](design.md#5-conformance-and-its-oracles) and [zugodb Q18][zdesign].

## Stage 9 — Parser hardening and the dialect reference

**Issue.** [#13](https://github.com/pedrobtz/libzugodb/issues/13),
sub-issue of [v0.3.0](https://github.com/pedrobtz/libzugodb/issues/9).

**CI.** The `fuzz` job gains a target.

**Goal.** SQL text is untrusted input the engine survives, and what the
engine accepts is written down.

**Work.**

- Quoted identifiers read the `""` escape; a parser error carries a
  position; no statement is skipped silently.
- The MySQL forms of design §3 follow DuckDB or are refused (L4).
- `fuzz/sql.cpp`: SQL text against a small fixed schema, seeded from the
  subset's statements.
- `docs/dialect.md`: statements, types, functions and the listed
  differences, with a check that every function it names is one the
  binder knows, and the reverse.

**Tests.**

- Identifier quoting: the cases of DuckDB's corpus for quoted
  identifiers, added to the subset; and a name holding `"` round-trips
  through `CREATE TABLE`, `INSERT` and `SELECT`, in
  `test/unit/parser/test_parser.cpp`.
- Each MySQL form: DuckDB's recorded result for the same text, or an
  error where DuckDB gives one, in
  `test/unit/execution/test_date_functions.cpp`.
- The SQL harness: no crash, hang or sanitizer report, and the same
  result at thread caps 1 and 2.
- `docs/dialect.md` against the binder's function table: a script the
  `ci` jobs run.

**Exit criteria.**

- The `fuzz` job is green on both targets, each at or above its reach
  floor.
- Every `**Tests.**` line exists and was seen to fail.
- The subset's counts did not get worse.

**Deviations (2026-11-20).**

- The stage grew past the parser, decided with an advisor (design L26):
  checking each MySQL form against DuckDB found wrong answers in the
  functions beside them, and the fuzzer in the binder, so the stage adds
  a function registry with DuckDB's argument counts and types, refuses
  every name outside it, and fixes or refuses what the checks found
  (`DATE_DIFF`, `STRFTIME`, `DAYNAME`, conditions of another type,
  `CASE`'s types, `GENERATE_SERIES`, `NTILE`, `DESCRIBE`, `SHOW`).
- Identifiers match ignoring ASCII case, as DuckDB's, which the roadmap did
  not name: two tables `t` and `T` was an accepted state DuckDB refuses.
  It let the subset reach cases it could not before (`Scoreboard` was
  `ScoreBoard` in `window/test_ntile.test`).
- `BEGIN`, `COMMIT` and `ROLLBACK` are refused (L26), beyond "no statement
  is skipped silently": with no undo, a statement after a failed one in a
  transaction was kept.
- The quoted-identifier cases added to the subset are eight files
  (`test/conformance/subset.txt`); most of their cases are refused (schemas,
  enums, structs), none is wrong or accepted.
- The fuzzer's reach is measured on `src/parser/parser.cpp`, the stage's
  subject, though it reaches the binder and executor too; the bugs it found
  were there. Its first run in the `fuzz` job reached 894 of the file's
  3,894 edges in 120 s; the floor is 800, about nine tenths of it, as
  `checkpoint_load`'s 240 is of its 276.
- The SQL harness's oracle is a class and a shape at thread caps 1 and 2,
  not values, as the subset's (L16).
- The counts, alike on both platforms: run 746 → 754 files (the eight
  added) and 20,194 → 20,355 cases; passed 5,625 (Linux) and 5,624 (macOS)
  → 5,733; refused 14,148 → 14,236; unreliable 421 and 422 → 386; wrong,
  accepted, crashed and hung 0. Sixteen cases that passed are now refused,
  on both platforms: eleven `BEGIN`, `COMMIT` and `ROLLBACK` statements in
  `window/test_tpcds_q49`, `window/test_window_tpcds`,
  `join/mark/test_null_safe_semi_uncommitted_stats` and
  `function/numeric/test_random` (L26); two `random()` calls in
  `test_random` (L25); and `YEARWEEK`, `WEEKDAY` and `WEEKOFYEAR` in
  `function/date/date_part_stats`, which passed over an empty table only
  because a function the executor lacks was never run on a row, and are
  now refused at bind time.

**Design.** [§3](design.md#3-dialect), [§4](design.md#4-parser) and L2, L4,
L26 in [§8](design.md#8-decisions).

## Stage 10 — Chunk capacity audit

**Issue.** [#14](https://github.com/pedrobtz/libzugodb/issues/14),
sub-issue of [v0.3.0](https://github.com/pedrobtz/libzugodb/issues/9).

**CI.** `ci`.

**Goal.** No operator writes past a chunk's capacity or drops rows at a
chunk boundary.

**Work.**

- The 31 chunks still created at the default capacity are audited
  ([zugodb §4.2][zdesign], zugodb#45): each is sized by what fills it, or
  shown to be filled in bounded steps.
- Release 0.3.0.

**Tests.**

- Each audited site, driven by a query whose intermediate result is 2047,
  2048, 2049 and 4097 rows, with a NULL on each side of the boundary:
  the row count and an order-independent checksum of the values equal
  those of the same query computed a second way (a `UNION ALL` of two
  halves). In `test/unit/execution/test_chunks.cpp`.
- The same file under ASan: no report.

**Exit criteria.**

- Every site in the audit's list has its test, and each was seen to fail
  or is recorded as already correct, with the reason.
- `v0.3.0` is tagged and released.

**Deviations (2026-11-20).**

- The audit counts 88 chunks, not 31: every `DataChunk::Initialize` in
  `src/`, 85 at the default capacity and 3 sized; zugodb's design gave a
  count of 31 and no list. Each is in L27's table with what
  bounds it and the case that drives it; none was seen to fail, so each is
  recorded as already correct with its reason. The sites no SQL reaches
  (`COPY`, live views, the CSV scans and the SQLite, Avro and Arrow
  readers) are recorded as unreachable, with what refuses them, not
  tested: Stage 11 removes them.
- Beyond the audit, a guard (L27): `Vector::SetValue` and
  `DataChunk::SetCardinality` past a vector's capacity are an
  `InternalException`, with doctests at the capacity and one past it.
- The tests run 4095 and 4096 rows as well, the second boundary's limit
  and limit minus one. The Parquet table case runs the roadmap's four
  sizes and leaves out the cases whose fast paths are wrong on a file the
  engine's writer made, at any size (#50; recorded, not fixed, as Stage 11
  removes them).
- The boundary tests found wrong answers that are not capacity bugs, and
  this stage fixes or refuses them, each with its issue and doctests:
  NULL keys grouped with 0 or `''` (#47); `|`-joined keys, set operations
  keeping repeated left rows, and an ignored `PARTITION BY` expression
  (#48); `MERGE` (#49), narrowed in `docs/dialect.md`.
- The SQL fuzzer's schema gains a 2049-row table, built through the C++
  API.
- `v0.3.0` is tagged and released by the maintainer after the merge, not
  in the pull request.
- The counts, alike on both platforms: passed 5,733 → 5,735, unreliable
  386 → 384; refused 14,236, wrong, accepted, crashed and hung 0, as
  before. The two are `GROUP BY` of `(g+i)%2` in
  `aggregate/group/group_by_all`, whose NULL key read a stale value (#47).
  No case moves from passed to refused.

**Design.** [zugodb §4.2][zdesign] and [zugodb Stage 8][zroadmap], L27 in
[§8](design.md#8-decisions).

## Stage 11 — Cut the engine's file readers

**Issue.** [#16](https://github.com/pedrobtz/libzugodb/issues/16),
sub-issue of [v0.4.0](https://github.com/pedrobtz/libzugodb/issues/15).

**CI.** `ci`.

**Goal.** The readers refused since 0.2.0 are no longer in the source,
and the engine carries no zstd of its own.

**Work.**

- The sizing first (LQ2): what in `src/storage/` and `src/execution/`
  depends on each reader, on zstd, snappy, fastpforlib and miniz, and what
  the database file's own compression needs. Recorded in the design.
- Remove the SQLite, Excel, Avro, Arrow IPC, CSV, JSON and Parquet
  readers, the operators and fast paths that only they feed, and each
  third-party library nothing else needs.
- The refusals of Stage 2 stay, now with nothing behind them.
- The changelog entry names every path removed, for zugodb's
  `tools/vendor.yaml`.

**Tests.**

- The refusal tests of Stage 2 pass unchanged.
- The conformance counts are equal to those before the cut.
- No symbol of a removed library is left: `nm` on the static library,
  for `ZSTD_`, `snappy` and `mz_`, in a CI step.
- A database file written by 0.3.0 reads back against its rows: a second
  golden file.

**Exit criteria.**

- Every `**Tests.**` line holds on all CI jobs.
- The line count removed is in the pull request, against the sizing.

**Deviations (2026-11-20).**

- The cut is wider than the readers, by L28: `src/storage/compression/`
  (SlothDB's column compression, which nothing called), `file_glob`,
  `hive_partition`, the catalog's file-scan and live-view fields,
  `CopyStatement` and `CreateViewStatement::is_live`, and the eleven
  execution helpers that only the fast paths called. `parser.cpp` is not
  touched.
- The sizing (L28): zstd 23,958 lines, snappy 4,793, fastpforlib 2,621,
  miniz 9,255, the readers 8,319, the helpers 4,278;
  `physical_planner.cpp` goes from 14,896 lines to 5,855 and
  `connection.cpp` from 3,438 to 2,305. In all, the pull request removes
  65,944 lines (33,305 under `src/` and `include/`, 31,373 under
  `third_party/`) and adds 685, documentation included.
- The refusal tests of Stage 2 in `test_refused.cpp` and
  `test_between_explain_view.cpp` pass unchanged; the 18 refusal cases of
  the deleted reader tests move into `test_refused.cpp` with their names
  and assertions, their Parquet files now the format's magic bytes.
- The thread, worker-error and chunk cases driven through the Parquet
  scan go with it; `WorkerSite::PIPELINE` goes, and `ParallelFor`, which
  no query reaches any more, is tested at its API.
- The second golden file, `test/fixtures/written_by_v0.3.0.db`, is byte
  for byte `golden_v1001.db`: the format has not changed since Stage 3.
- The symbol check is `tools/check_symbols.sh`, in each `test` leg and
  `mingw`; it also checks fastpforlib's and miniz's names and that the
  removed paths stay removed.
- #50 (the Parquet fast paths' wrong answers) closes with the code.

**Design.** [§2](design.md#2-scope), LQ2 in
[§9](design.md#9-open-questions) and [zugodb §7.5][zdesign].

## Stage 11.1 — Host-supplied function providers

**Issue.** [#40](https://github.com/pedrobtz/libzugodb/issues/40),
sub-issue of [v0.4.0](https://github.com/pedrobtz/libzugodb/issues/15).
Added 2026-11-20 (L25).

**CI.** `ci`.

**Goal.** The engine calls functions it does not implement through
versioned tables its host supplies, and refuses them when no table is
given.

**Work.**

- `zugodb_providers`: one table pointer per service (regex, UTF-8; UUID
  and random later), each with an ABI version and `struct_size`, set once
  per `Database`.
- The regex functions refused by L24 go through the regex table, under
  zuregex's RE2 dialect (zuregex#211). A constant pattern is compiled once
  per query, with match data per worker thread, and a hit limit is an
  error.
- UTF-8 validation and counts go through the UTF-8 table where present,
  with the built-in code as the fallback.
- Every non-OK status a provider returns is a query error.

**Tests.**

- A fake provider in `test/`: a null table, a wrong ABI version, a short
  `struct_size`, a status turned into an error, and calls at thread caps
  1, 2 and 4 under TSan.
- The conformance counts are unchanged, with no provider.

**Exit criteria.**

- Every `**Tests.**` line holds on all CI jobs.

**Design.** L25 in [§8](design.md#8-decisions).

## Stage 12 — The names become zugodb

**Issue.** [#17](https://github.com/pedrobtz/libzugodb/issues/17),
sub-issue of [v0.4.0](https://github.com/pedrobtz/libzugodb/issues/15).

**CI.** `ci`.

**Goal.** Nothing a user of the library sees is named after SlothDB,
except the credit.

**Work.**

- `include/slothdb/` becomes `include/zugodb/`; `SLOTHDB_*` macros and
  CMake options become `ZUGODB_*`; the targets become `zugodb` and
  `zugodb_tests`; the shell becomes `zugodb`.
- The README, `CONTRIBUTING.md`, `CLAUDE.md` and CI follow.
- The credit stays: `LICENSE`, the README's origin paragraph and
  `docs/SLOTHDB-README.md`.
- Release 0.4.0, with the renames listed for zugodb's vendoring.

**Tests.**

- No `slothdb` or `SLOTHDB` outside the credit and the changelog's
  history: a grep with those paths excluded, as a CI step.
- The rename changes no result: the doctest suite and the conformance
  counts are unchanged.
- The exported symbols of the static library differ from 0.3.0's only by
  name: an `nm` listing before and after, shown in the pull request.

**Exit criteria.**

- Every `**Tests.**` line holds on all CI jobs.
- `v0.4.0` is tagged and released.

**Design.** [§7](design.md#7-the-contract-with-zugodb).

## After 0.4.0

- [zugodb §16.5][zdesign] steps B to D: a table source registry and one
  generic scan over a stream, registered scalar functions, results out as
  Arrow, and the public C header of §16.3. They follow zugodb 0.1.0 (Q19,
  Q21).
- Splitting `physical_planner.cpp`, `expression_executor.cpp` and
  `connection.cpp`: only with the conformance job holding the counts
  ([§6](design.md#6-what-the-repository-holds)).
- A wider subset and a higher pass rate. The rate is reported, not
  targeted ([§5](design.md#5-conformance-and-its-oracles)).
- The optimizer, transactions with undo, and cancellation
  ([§2](design.md#2-scope)).
- Another parser ([§4](design.md#4-parser)): reopened only if a sibling
  package has one the engine could link.

## Blocking open questions

- LQ1 in [§9](design.md#9-open-questions) (the DuckDB tag and the first
  subset) is settled in Stage 6.
- LQ2 (what the cut takes with it) is the first work of Stage 11.

[zdesign]: https://github.com/pedrobtz/zugodb/blob/main/.agents/design.md
[zroadmap]: https://github.com/pedrobtz/zugodb/blob/main/.agents/roadmap.md
