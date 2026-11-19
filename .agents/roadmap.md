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
| 1 | [Errors on worker threads become query errors](#stage-1--errors-on-worker-threads-become-query-errors) | 0.2.0 | ci | [#3](https://github.com/pedrobtz/libzugodb/issues/3) | | |
| 2 | [Refuse the CSV, Parquet and JSON table functions](#stage-2--refuse-the-csv-parquet-and-json-table-functions) | 0.2.0 | ci | [#4](https://github.com/pedrobtz/libzugodb/issues/4) | | |
| 3 | [A checksum for the database file, and format version 1001](#stage-3--a-checksum-for-the-database-file-and-format-version-1001) | 0.2.0 | ci | [#5](https://github.com/pedrobtz/libzugodb/issues/5) | | |
| 4 | [Harden Checkpoint::Load, with a fuzz harness in CI](#stage-4--harden-checkpointload-with-a-fuzz-harness-in-ci) | 0.2.0 | + `fuzz` | [#6](https://github.com/pedrobtz/libzugodb/issues/6) | | |
| 5 | [ThreadSanitizer in CI](#stage-5--threadsanitizer-in-ci) | 0.2.0 | + `linux-tsan` | [#7](https://github.com/pedrobtz/libzugodb/issues/7) | | |
| 6 | [Conformance runner and first counts](#stage-6--conformance-runner-and-first-counts) | 0.3.0 | + `conformance` | [#10](https://github.com/pedrobtz/libzugodb/issues/10) | | |
| 7 | [No crash and no hang on the pinned subset](#stage-7--no-crash-and-no-hang-on-the-pinned-subset) | 0.3.0 | ci | [#11](https://github.com/pedrobtz/libzugodb/issues/11) | | |
| 8 | [No wrong answer on the pinned subset](#stage-8--no-wrong-answer-on-the-pinned-subset) | 0.3.0 | ci | [#12](https://github.com/pedrobtz/libzugodb/issues/12) | | |
| 9 | [Parser hardening and the dialect reference](#stage-9--parser-hardening-and-the-dialect-reference) | 0.3.0 | `fuzz` gains a target | [#13](https://github.com/pedrobtz/libzugodb/issues/13) | | |
| 10 | [Chunk capacity audit](#stage-10--chunk-capacity-audit) | 0.3.0 | ci | [#14](https://github.com/pedrobtz/libzugodb/issues/14) | | |
| 11 | [Cut the engine's file readers](#stage-11--cut-the-engines-file-readers) | 0.4.0 | ci | [#16](https://github.com/pedrobtz/libzugodb/issues/16) | | |
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

**Design.** [§5](design.md#5-conformance-and-its-oracles) and
[zugodb Q18][zdesign].

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

**Design.** [§3](design.md#3-dialect), [§4](design.md#4-parser) and L2, L4
in [§8](design.md#8-decisions).

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

**Design.** [zugodb §4.2][zdesign] and [zugodb Stage 8][zroadmap].

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

**Design.** [§2](design.md#2-scope), LQ2 in
[§9](design.md#9-open-questions) and [zugodb §7.5][zdesign].

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
