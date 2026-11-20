# Changelog

## libzugodb 0.3.0 (in development)

- A conformance runner, `tools/slt/` (`zugodb_slt`), runs DuckDB's own
  sqllogictest files through `Connection::Query` and classifies each case
  as passed, refused, wrong, accepted, unreliable, crashed or hung, one
  child process per file. The pinned subset is DuckDB v2.0.0's
  `test/sql` under `aggregate`, `filter`, `join`, `order`, `subquery`,
  `window`, `types` and `function/{numeric,string,date}`; its first counts
  are committed in `test/conformance/`, and a new CI job, `conformance`,
  fails when a run's counts differ from them. Nothing in `src/` or
  `include/` changes.
- No case of the pinned subset crashes or hangs, under ASan and UBSan
  (#11):
  - a string function called with too few or too many arguments
    (`sha1()`, `REPLACE(1)`, `LTRIM('a', 'b', 'c')`, ...) is a binder
    error, and every scalar function checks each argument it reads;
  - `LPAD` and `RPAD` count characters, cut a long string to its first
    characters, give `''` for a length of zero or less (it hung), refuse an
    empty padding and a result past the maximum string size;
  - `INSERT INTO t SELECT ... FROM t` reads the table as it was when the
    statement began; it read its own rows and never finished (#23);
  - in a query with window functions, `ORDER BY` sorts by the output
    column it names, or by the source columns; it read the window's
    output rows at the source's column positions, past their end or at the
    wrong column. Other `ORDER BY` items there are refused;
  - `rank()` and `dense_rank()` with no `ORDER BY` are 1 for every row;
  - `LENGTH(NULL) ... GROUP BY NULL` is NULL: a column reference to a type
    with no storage lost its NULLs. A column reference past the end of its
    input is an error;
  - `GROUP BY a` and `HAVING a` bind to the column `a` when a select-list
    alias is also named `a`, as in DuckDB; a `HAVING` alias over a column
    of its own name recursed without end;
  - `date_trunc` reads its part per row, so the part may be a column or
    NULL, and refuses a part it does not know (it returned the value cut
    to the second);
  - `substring` takes `BIGINT` bounds, which overflowed an `INTEGER`
    buffer, and bounds past the 32-bit range.
- Fewer wrong answers on the pinned subset (#12; the rest are sub-issues
  of v0.3.0). Each was fixed, or its shape is now an error:
  - a join whose `ON` has no equality between a column of each side
    (`ON TRUE`, `ON i = 1`, `ON i < k`, `ON a OR b`) joined on the first
    column of each side; it is now checked pair by pair. The fused join
    and aggregate path ignored every conjunct after the first, and a
    `NULL` key matched the string `'NULL'`;
  - `NATURAL JOIN` (it joined on the first columns) and a join other than
    `CROSS` without `ON` are errors;
  - text to `DATE`, `TIME` and `TIMESTAMP` is read by DuckDB's grammar
    (design L19): every `TIME` from text was 00:00:00 and `TIME '...'`
    was refused (#25), `2001-02-29` became 2001-03-01, and `TRY_CAST` gave
    NULL for `2004/02/29`. What DuckDB may read but the engine cannot
    (`infinity`, a year outside 1 to 9999, a time zone) is an error, under
    `TRY_CAST` too. Dates before year 1 print with ` (BC)`, fractions of
    a second without trailing zeros;
  - `DECIMAL` and `NUMERIC` are refused (#26): they were `DOUBLE`s. So are
    `TIMESTAMP_S`, `_MS`, `_NS`, `TIMESTAMPTZ`, `TIMETZ` and `TIMESTAMP(p)`
    other than 4 to 6, which were the engine's `TIMESTAMP`;
  - window functions: every shape the window operator does not compute is
    refused (design L21), where it gave NULL, read an expression argument
    as 0, ignored `DISTINCT`, `FILTER` or `LAG`'s default, found peers by
    the first `ORDER BY` key, or took the whole partition as the frame;
    window sums over integers are exact, and NULL over no value;
  - `SUBSTRING`, `LEFT`, `RIGHT`, `INSTR`, `STRPOS`, `TRANSLATE` and
    `LIKE`'s `_` count characters, `STRLEN` bytes, with DuckDB's
    `SUBSTRING` offsets and bounds; `UPPER`, `LOWER`, `ILIKE`, `LENGTH`
    and `REVERSE` are refused where Unicode case mapping or grapheme
    clusters could change the answer (design L22); `REPEAT`'s count must
    be an integer;
  - `STRING_AGG` honours its separator; `MIN` and `MAX` of a `BOOLEAN`
    and `SUM` of a `TINYINT` or `SMALLINT` read their values (they were
    0); an aggregate the engine skipped (`MIN('hello')`, `STRING_AGG` of
    an expression, DuckDB's `mode`, `any_value`, `product`, ...) is an
    error, and so is a `BIGINT` `SUM` past 2^53, which was rounded;
  - a `TINYINT` or `SMALLINT` operand of floating-point arithmetic was
    read as a double's bytes (`1::TINYINT + 1::REAL` was 1).
- No wrong answer is left on the pinned subset (#31 to #38, closing
  #12). Each cause was fixed, or its shape is now an error:
  - `BLOB` (#31): text to `BLOB` reads `\xHH` as one byte and refuses any
    other escape or non-ASCII byte (NULL under `TRY_CAST`); a `BLOB`
    prints as DuckDB's text and stays a `BLOB` in a `Value`. A cast to
    `BLOB` from anything but text, `||` or a comparison of a `BLOB` with
    a non-`BLOB` other than a string literal, and any function of a
    `BLOB` but `COUNT`, `MIN`, `MAX`, `OCTET_LENGTH`, `MD5`, `SHA1` and
    `SHA256` are errors; `MIN` and `MAX` compare its bytes;
  - regular expressions (#32): every regular expression function and
    operator (`regexp_matches`, `regexp_replace`, `regexp_extract`,
    `regexp_full_match`, `regexp_extract_all`, `regexp_split_to_array`,
    `SIMILAR TO`, `~`, ...) is refused, with an error naming #40, until a
    host-supplied RE2 provider supplies them (design L24); `std::regex`
    gave ECMAScript's answers over bytes, and is no longer used;
  - floating point (#33): `/` divides as doubles, as DuckDB's (`7 / 2`
    is 3.5), and by zero gives an infinity, not NULL; `^` is `POWER`, not
    the bitwise exclusive or; `POWER`, `SQRT`, `CBRT` and `ROUND` follow
    IEEE and DuckDB. The engine holds no NaN (design L23, until #42): a NaN is an
    error wherever it would be made, stored, read from a database file or
    returned, since DuckDB orders NaN above every number and the engine's
    comparisons are IEEE's. `SUM` of a `FLOAT` is a `DOUBLE` (it was 0,
    #36);
  - arguments DuckDB refuses (#34): an integer `%` or `MOD` by zero and
    an integer overflow are errors, not NULL; arithmetic takes numbers
    only (`1::TINYINT + 1::VARCHAR` was 1); a `BIGINT` past 2^53 in
    floating-point arithmetic is refused; `FACTORIAL` of a negative or
    past 20!, `GCD`, `LCM` and `FACTORIAL` of a non-integer, `CHR` of a
    code point outside Unicode, `CONCAT_WS` of fewer than two arguments,
    `CONTAINS(NULL, NULL)`, a `LIKE` pattern ending in its escape
    character, a parameter on a type that takes none and `FLOAT(p)`
    outside 1 to 53 are errors; `FLOAT(25)` to `FLOAT(53)` is a `DOUBLE`;
  - `ORDER BY` (#35): a non-integer literal is DuckDB's error; over a
    set operation a key other than an output name or position, which was
    skipped, is refused; a negative `LIMIT` or `OFFSET` is an error.
    NULLs sort last after `DESC` too, DuckDB's `default_null_order`; a
    window's `ORDER BY` placed a NULL key by its bytes, `ORDER BY x LIMIT
    k` dropped the rows whose key is NULL, and with two keys it chose
    among the first key's ties as they came;
  - `EXTRACT(WEEK)` is the ISO week near the start of a year (#37);
  - shapes (#38): `DATE`, `TIME` and `TIMESTAMP` values compared as
    neither equal nor less, so sorts and `UNION` through values left them
    as they came; `ORDER BY` a position of `SELECT *` over a join was the
    other table's column of that name; a recursive CTE's step read the
    whole table so far, not the step before's rows (a count to 3 made
    500,501 rows); `GROUP BY` a join key made a group with `COUNT(*)` 0
    for a key nothing joined; more column aliases than a subquery's
    columns are an error.
- `CREATE TABLE` returns DuckDB's result, a `Count` column, with the rows
  made by `CREATE TABLE AS` and none for a plain `CREATE TABLE`; it
  returned no column. `QueryResult::is_count` marks the result of
  `INSERT`, `UPDATE`, `DELETE` and `CREATE TABLE`, so a caller reads rows
  affected from it rather than from the column's name. zugodb's
  `EngineSend` treats a non-DML `Count` result as a query today, and
  adapts at its next pin. For `tools/vendor.yaml`, two files are new:
  `include/slothdb/common/types/blob.hpp` and
  `include/slothdb/common/types/nan_guard.hpp`.

  The conformance runner leaves out a query whose expected result is a
  regex in any column (an `EXPLAIN` plan; design L20), and splits a
  recorded row as DuckDB's runner does, dropping empty pieces.
- Remove `bench/correctness/`, SlothDB's Python harness, which ran
  SlothDB's published wheel rather than this tree.
- Integer arithmetic overflows as DuckDB's does (#43). `+`, `-`, `*` and
  `%` of `TINYINT` and `SMALLINT` compute in the wider operand's type and
  raise on overflow; they widened to `INTEGER`, so `100::TINYINT +
  100::TINYINT` was 200 where DuckDB raises. An integer literal takes
  the other operand's type when it fits it, as DuckDB's does, so `x + 1`
  of a `TINYINT` column raises at 127, and `-2147483648` is an `INTEGER`.
  Wrong values with no error are gone at every width: `2147483647 * 2`
  gave -2 (the overflow check was optimised away), the negation of a
  type's least value gave itself, its `% -1` gave 0, `SUM(x + 1)` added
  `COUNT(*)` to `SUM(x)` and missed a row that overflows, and `<<` and
  `>>` follow DuckDB (`1 << 31`, a negative operand or count past the
  width are errors; they wrapped or gave NULL). `/` stays a `DOUBLE`
  division. `ABS` of a type's least value raises (it was NULL).

  Found with it: a numeric `Value` read as another numeric type converts
  it, where it read the bytes of another member of a union, so `TRUNC`
  of anything but a `DOUBLE` was 0 and `CEIL`, `FLOOR` and `ROUND` of a
  `TINYINT` or `SMALLINT` were 0. `LN`, `LOG`, `LOG2`, `LOG10` and `EXP`
  give DuckDB's infinities (`ln(0)` is `-inf`, `exp(1000)` is `inf`)
  where they gave NULL, and refuse a NaN.
- The parser is hardened and the dialect written down (#13, design L26):
  - quoted identifiers read the `""` escape, so a name may hold `"`;
    backticks delimit a name as in DuckDB; an empty quoted name is an
    error. Names match ignoring ASCII case and keep their spelling, as
    DuckDB's: `SELECT foo FROM Foo` works and names its column as the
    table does, `t` and `T` are one table, and two columns of one name are
    refused (or renamed `name_1` in a subquery, a view or `CREATE TABLE
    AS`). A database file holding two such names, or an empty one, is
    refused when opened;
  - a parser error's message ends with its line and column; a statement
    ends at `;` or the end of the text (`SELECT 1 SELECT 2` ran as two);
    a CTE's column aliases name its columns (they were skipped, so `WITH
    t(b, a) AS (SELECT 1 AS a, 2 AS b) SELECT a FROM t` gave 1);
  - every function is in one registry with DuckDB's argument counts and
    types, and any other name, count or type is an error at bind time.
    Refused: MySQL's and other forms DuckDB does not have (`NVL`, `IIF`,
    `GETDATE`, `CURDATE`, `DATE_FORMAT`, `DATEADD`, `TIMESTAMPADD`,
    `TIMESTAMPDIFF`, `SUBDATE`, the two-argument `DATEDIFF`, ...),
    `RANDOM` and `UUID` (a host's provider, L25), `NOW`,
    `CURRENT_TIMESTAMP`, `CURRENT_DATE`, `TO_TIMESTAMP` and `AGE` (a time
    zone or a type the engine has not), `DATE_ADD`, `DATE_SUB`, `VERSION`
    and `CURRENT_DATABASE`, `TIMESTAMP ± INTEGER`, `TIMESTAMP - TIMESTAMP`
    and `DATE + BIGINT`;
  - `DATE_DIFF` counts part boundaries as DuckDB's (`date_diff('month',
    2020-01-31, 2020-02-01)` is 1, not 0); `STRFTIME` formats DuckDB's
    specifiers (the C library's `%c`, `%x`, `%g` and `%z` differed);
    `DAYNAME` and `MONTHNAME` of a `DATE` are right (every `DATE` was a
    Thursday in January); `NTILE` with more buckets than rows, and a count
    below 1, are DuckDB's;
  - a condition of another type is cast to `BOOLEAN` (`NOT 10` was true,
    `CASE WHEN 2` took the `ELSE`); `CASE`, `COALESCE`, `NULLIF`,
    `GREATEST` and `LEAST` keep `BOOLEAN`, `DATE` and `FLOAT` branches as
    they are (they were `INTEGER` or `DOUBLE`); `GENERATE_SERIES` takes
    negative literals and steps;
  - `BEGIN`, `COMMIT` and `ROLLBACK` are refused: with no undo, a
    statement after a failed one in a transaction was kept where DuckDB
    rolls it back. `DESCRIBE` and `PRAGMA table_info` report `NOT NULL`
    and `PRIMARY KEY`, `SHOW TABLES` is in DuckDB's order, and `PRAGMA
    database_list`, `SHOW COLUMNS` and `SHOW ... LIKE` are refused;
  - `docs/dialect.md` lists the statements, types and functions accepted
    and what is refused; `tools/check_dialect.sh`, run by the `ci` jobs,
    checks its functions against the binder's. A libFuzzer harness over
    SQL text, `fuzz/sql.cpp`, joins the `fuzz` job, seeded from the
    subset's statements (`fuzz/make_sql_corpus.sh`). The subset gains
    DuckDB's files on quoted identifiers and identifier case. For
    `tools/vendor.yaml`, no path under `src/` or `include/` is added or
    moved.

## libzugodb 0.2.0

A minor version, as the database file's format changes (version 1001,
with a checksum). SQL reads no file, a damaged database file is refused,
an error on a worker thread is an error of the query, `HUGEINT`,
`INTERVAL` and `UUID` are refused, two data races are fixed, and CI runs
a fuzzer and ThreadSanitizer.
For zugodb's `tools/vendor.yaml`, one file is new,
`include/slothdb/common/crc32.hpp`; `src/api/` and `include/slothdb/api/`
are removed (zugodb never vendored them).

- Remove SlothDB's C API, `src/api/` and `include/slothdb/api/`, which
  served its Python binding. zugodb never vendored it. The `slothdb` shell
  now uses `Database` and `Connection`.
- Remove what was SlothDB's product and not the engine: the website and
  WASM playground under `docs/` (`docs/SLOTHDB-README.md` stays),
  `packaging/`, `python/`, `cloudflare/`, `assets/`, `install.sh`,
  `scripts/build-wasm.sh`, `tools/playground/`, `examples/`, the performance benchmarks
  under `bench/` (`bench/correctness/` stays), `.github/codeql/` and
  `TODO-2026-04-29.md`.
- Remove the `third_party/llama.cpp` submodule and `.gitmodules`, and the
  unused copy of `fastpforlib` at the top of `third_party/`.
- Remove the CMake options `SLOTHDB_ASK_MODEL`, `SLOTHDB_EDGE` and
  `SLOTHDB_BUILD_SHARED`.
- An exception on an engine worker thread is an error of the query, not
  `std::terminate`: a damaged Parquet page (SouravRoy-ETL/slothdb#33), or
  an error in any other parallel phase of a reader or an operator. Every
  thread start in `src/` goes through `WorkerGroup`, `ParallelFor` or
  `WorkerErrors` in `include/slothdb/common/parallel.hpp`, which keep the
  first exception and rethrow it on the query's thread after the join;
  the other workers stop early. `parallel.hpp` also holds a test seam,
  `ArmWorkerFault`, that costs one relaxed atomic load per worker.
- SQL reads and writes no file: files reach the engine as Arrow C streams,
  through zugodb. The parser refuses, before anything is opened,
  `read_csv`, `read_csv_auto`, `read_parquet`, `read_json`,
  `read_json_auto`, `read_ndjson` and the other file table functions of
  DuckDB, a file path used as a table (any extension, globs), `DESCRIBE`
  of either, `COPY` to or from a file in every format, and `CREATE LIVE
  VIEW`, with one message: "Reading or writing files from SQL is not
  supported". A batch that holds such a statement runs none of its
  statements. This also closes a wrong answer: a `GROUP BY` over a Parquet
  file written by `COPY ... TO` returned no rows. The readers stay, tested
  through their C++ API, until 0.4.0 removes them.
- The database file's format changes, which is why this is a minor
  version: files are format version 1001, libzugodb's own range, and carry
  a CRC-32 of the whole file in the header (offset 12, the field upstream
  left reserved). Opening a file refuses, before any table is read, one
  smaller than its header, a bad magic number, any version other than
  1001 (the message names the version found and 1001, so a file written by
  upstream SlothDB or by libzugodb 0.1.x, version 1, is refused), a size
  other than the header records, and a checksum that does not match. Such
  a file was read before and could open with wrong rows. Format 1001 is
  little-endian, and a big-endian host refuses to read or write a database
  file. New file for zugodb's `tools/vendor.yaml`:
  `include/slothdb/common/crc32.hpp`, a header-only CRC-32 (zlib's).
  `Checkpoint::ComputeChecksum` is public, for a fuzz harness.
- A database file that passes the checksum but was not written by `Save`
  is refused, not misread: `Checkpoint::Load` checks every length, count
  and offset against the bytes left before it reads or allocates, the
  header fields `Save` writes as constants, each column type (only those a
  table made by SQL can have), each null flag, `BOOLEAN` and `SMALLINT`
  range, a table name given twice and bytes after the last table, with a
  "Damaged database file" message naming the field and its offset.
  Non-NULL `HUGEINT`, `INTERVAL` and `UUID` values, which `Save` writes as
  text the loader never read back correctly, are refused; their NULLs
  load. What a load allocates follows the file's size: each table's row
  groups are sized to its rows (`RowGroup` takes a capacity). A refused
  file adds no table to the catalog. The 100,000-table and billion-row
  caps go, replaced by these bounds. Found by fuzzing the loader: a
  `TIME` of `INT64_MIN` overflowed when rendered, and a `DATE` or
  `TIMESTAMP` far from 1970 overflowed or showed the wrong year; they
  render correctly now.
- SQL refuses the types `HUGEINT`, `INTERVAL` and `UUID`, in every
  database, in-memory included: `CREATE TABLE`, `ALTER TABLE ... ADD
  COLUMN`, `CAST`, `TRY_CAST` and `::` fail with "<type> is not
  supported". None held a correct value: a `HUGEINT` or `UUID` came back
  as a different garbage number on each run (so `WHERE h = 5` matched
  nothing), and an `INTERVAL` could not hold any non-NULL value; and the
  database file has no encoding for them, so a file holding one could not
  be opened again. A file written before keeps loading their NULLs.
- A string type's length, `VARCHAR(n)`, `CHAR(n)`, `TEXT(n)` or
  `STRING(n)`, has no effect, as in DuckDB: the column is `VARCHAR` and
  `DESCRIBE` shows `VARCHAR`. SlothDB refused an over-long string on
  `INSERT ... VALUES` only, and the length was lost when the database was
  saved; a length past 64 bits threw a C++ `std::out_of_range`.
  `VarcharTypeInfo` and `LogicalType::VARCHAR_N` are removed. `fuzz/` holds the libFuzzer harness
  (`-DSLOTHDB_FUZZ=ON`, clang), its seed corpus and scripts, and CI runs
  it on every pull request and weekly; zugodb does not vendor it.
- The window operator no longer races on its per-partition "sorted" flags.
  From 200,000 rows and two partitions it sorts partitions on several
  workers, and the flags were a `std::vector<bool>`, whose neighbours share
  a word, so two workers wrote one word at once (a data race, undefined
  behaviour; a lost flag only made a partition sort twice). They are one
  byte each now. Found by ThreadSanitizer.
- The Parquet scan's consumer advances its row-group counter under the
  mutex its throttled decode workers read it with; it was incremented
  after the lock was released, a data race, and the woken worker saw the
  old value. Found by ThreadSanitizer.
- CI runs the suite under ThreadSanitizer (`linux-tsan`, CMake option
  `SLOTHDB_TSAN`), with a loop of multi-threaded queries at thread caps 2
  and 4 whose results must equal those at cap 1.

## libzugodb 0.1.1

- Parquet decode workers no longer read the worker list while the main
  thread is still appending to it. `WorkerLoop` read `workers_.size()` as
  the workers started, a data race that ThreadSanitizer reports on every
  multi-threaded Parquet scan, and that libstdc++ 15 under UBSan stops as
  "execution reached an unreachable program point". The count is now set
  before the first worker starts.

## libzugodb 0.1.0

The first release as libzugodb: SlothDB at `8180f21` (v0.2.7+) with these
changes, in order. Their reasons are in the commit messages.

- Take the Windows branch under MinGW, not only MSVC.
- Write nothing to stderr, never abort, use no rand().
- Refuse remote paths: offline only.
- Cap every thread pool with one process-wide limit.
- Refuse STRFTIME with one argument.
- Fix two warnings R CMD check counts.
- Add a read-only database mode.
- Set stop flags under the mutex their waiters use.
- Keep the Parquet decoder's scratch out of thread_local.
- Size validity masks by their holder's capacity.
- Update seven doctests to DATE as days since 1970.
- Make ROLLBACK say that nothing was undone.
- Refuse Parquet columns the reader cannot decode faithfully, when bound.
- Refuse JSON the readers would misread, and decode \u escapes.
- Refuse Arrow IPC: SlothDB's Arrow format is its own.
- Refuse the SQLite, Excel and Avro readers.
- Rename the C++ namespace from `slothdb::` to `zugodb::`.
- Remove the HTTP client, the extension loader and the `ask` module, with
  their tests and the shell's `.ask` command: the engine is offline and
  loads no code (378 doctest cases remain).

## SlothDB

The entries below are SlothDB's own.

## 0.2.3

### Bug fixes

- `read_parquet('dir/*.parquet')` and bare `FROM 'dir/*.parquet'`
  now expand globs and read every matching shard, instead of
  failing with `IO Error: Cannot open Parquet`. Single-file paths
  keep the streaming PhysicalParquetScan fast path; multi-file
  globs bulk-load matching shards into an in-memory table after
  validating column counts match the first file. Fixes #6 (WASM
  playground couldn't read multi-file Parquet datasets).

## 0.2.2

### Performance

- Stats-based row-group pruning for top-N pushdown. Pass 1 of the
  two-pass top-N now reads each row group's max (or min, for ASC)
  from the Parquet footer, sorts RGs best-first, and decodes
  sequentially. The heap converges to the global K-th best after
  one or two RGs; the rest are skipped without ever touching their
  column data on disk. `parquet10m_orderby_top10`: 69 ms to 15 ms.
  Now faster than DuckDB (23 ms), goes from SLOW to WIN.
- Direct `string_t` emit in PhysicalWindow. The vectorised emit
  fast path was going through `Value::VARCHAR` boxing for every
  row. Now writes `string_t` straight into the output vector's
  typed buffer. 10M-row varchar emit drops from ~3 s to ~300 ms.
- Lazy column-major Python results. `QueryResult` now holds the C
  result pointer alive until garbage-collected. Two new methods:
    - `fetchnumpy()` returns a dict of numpy arrays. Numeric
      columns wrap the C buffer via `np.frombuffer` with a
      defensive copy; varchar becomes a numpy object array.
    - `fetchdf()` builds a pandas DataFrame from `fetchnumpy()`,
      bypassing row-tuple materialisation entirely.
  10M-row int conversion drops from ~4 s to ~50 ms.

### Bench scoreboard against DuckDB

13 win, 5 tie, 0 slow on the 18-query suite. Up from 11 / 5 / 2
in 0.2.1.

### Tests

403 of 403 doctest passing on Windows, Linux, macOS.

## 0.2.1

### Fixes

- Avro reader misaligned every nullable column (issue #5). Fields
  declared as `["null", T]` unions ship with a 1-byte union index
  prefix on every value. The reader was skipping that byte, so for
  `["null", {"type":"long","logicalType":"timestamp-millis"}]` the
  index byte was being read as the long value and every subsequent
  column drifted. The reader now reads the union index, treats index 0
  as null, and propagates that into the chunk validity mask.
- Avro logical types: `timestamp-millis`, `timestamp-micros`, `date`,
  `time-millis`, `time-micros`. The schema parser now recognises
  logical-type wrappers, both directly
  (`{"type":"long","logicalType":"timestamp-millis"}`) and inside
  unions. timestamp-millis is rescaled from milliseconds to
  microseconds at decode time to match SlothDB's internal TIMESTAMP
  representation.
- `max(timestamp_col)` and `max(date_col)` returned the first row.
  The aggregate fast-path comparator only handled INTEGER, BIGINT,
  FLOAT, DOUBLE. For TIMESTAMP, DATE, TIME, TIMESTAMP_TZ it returned
  0.0 always, so every row past the first compared as `0 > 0` and
  the first row was the running max forever. Added the date and time
  logical types to both the comparator and the group-key builder.
- DATE, TIMESTAMP, TIME now render as ISO-8601 strings. `Value::ToString`
  was producing raw integers (days, microseconds). DATE now formats as
  `YYYY-MM-DD`, TIMESTAMP as `YYYY-MM-DD HH:MM:SS[.uuuuuu]`, TIME as
  `HH:MM:SS[.uuuuuu]`. The date conversion is reentrant and avoids
  `gmtime`. `Vector::GetValue` no longer drops the LogicalType for
  these types so aggregates keep their typing through the planner.

### Performance

- Chunk-resident QueryResult. SELECT queries now stream typed
  DataChunks straight into `QueryResult::chunks` instead of boxing
  every cell into a Value during result collection. The C API typed
  batch fetch (`slothdb_column_*_buffer`) reads from those chunks via
  memcpy with no intermediate object materialisation. The legacy
  per-cell API (`GetValue`, `ToString`, set ops) lazily materialises
  rows on first access so behaviour is unchanged.

  Bench impact, 10M-row window queries returning to Python:

  | Query             | Before     | After      | DuckDB    |
  |---|---:|---:|---:|
  | parquet10m_window | 18,801 ms  | 12,304 ms  | 7,449 ms  |
  | csv1m_window      | 1,741 ms   | 1,031 ms   | 886 ms    |
  | avro1m_window     | 1,577 ms   | 991 ms     | 1,337 ms  |

- Row-limit pushdown into Parquet scan. A bare `LIMIT N` now clips
  `effective_num_rgs_` in `Init` so workers and the emit loop only
  iterate row groups whose cumulative row count covers the limit.
  `SELECT region, qty FROM file LIMIT 100000` over 10M rows:
  11,500 ms to 132 ms (87x faster). `SELECT * LIMIT 10` 7x faster.
  Skipped when pushdown filters are active.
- Two-pass top-N pass 2 parallelism. Pass 2 now decodes column
  chunks at `(rg, col)` granularity rather than `(rg)`, so a single-RG
  winner set still uses every available core. The key column is no
  longer re-decoded since pass 1 already captured those values.
  `parquet10m_orderby_top10`: 83 ms to 69 ms.

### Tests

- New `sloth-test/test_avro_repro.py` reproduces issue #5 end-to-end.
- New `sloth-test/test_format_smoke.py` runs SELECT, COUNT, MAX,
  WHERE, ORDER BY+LIMIT across CSV, JSON, Parquet, SQLite, Avro, and
  a CSV+JSON join. 22 of 22 passing. (Arrow IPC is skipped: the
  reader uses a SlothDB-bespoke on-disk format, not the Apache Arrow
  Flatbuffers IPC format that pyarrow emits. Pre-existing limitation,
  not a regression.)

## 0.2.0 — performance overhaul, predicate + top-N pushdown, typed Python batch fetch

A wide perf and correctness pass. The 18-query bench against DuckDB went from 3 wins / 6 ties / 9 slow to 11 wins / 5 ties / 2 slow. Several queries that were 50-6000x slower than DuckDB are now within 1.2x or beat it.

### Performance

- **Top-N pushdown for `ORDER BY ... LIMIT N`.** Bounded-heap operator instead of full sort + truncate. 10M-row Parquet `ORDER BY q DESC LIMIT 10`: 420,344 ms -> 119 ms.
- **Two-pass top-N for Parquet.** Pass 1 decodes only the order-by column to find the top-K row indices; pass 2 fetches just those rows from the few RGs that contain them. Pass 2 is parallelised across unique RGs.
- **Parquet row-group statistics pruning.** WHERE clauses pushed into the scan; row groups whose min/max prove no row matches are skipped without decoding. Three latent bugs fixed along the way: stats not propagated from the Thrift parser, fields 5/6 swapped vs the Parquet spec, and cross-type Value comparison silently returning false. `WHERE quantity > 999` (no rows match): 4,984 ms -> 2 ms.
- **Streaming `FROM 'file.parquet'`.** The bare-string-literal Parquet path was materialising every row into an in-memory DataTable before scanning. Now uses the streaming PhysicalParquetScan, identical to `read_parquet()` form. `count(*)`: 90,000 ms -> 0.8 ms.
- **Fused `count(*) WHERE pred OVER PARQUET`.** When the planner sees AGG -> FILTER -> PARQUET with a compileable predicate, runs RunParallelRGs and counts matches per-row inside the worker, skipping PhysicalFilter's row-copy pass entirely. Single-predicate inner loop is hoisted to a per-(type, op) tight loop the compiler auto-vectorises. 4,984 ms -> 76 ms.
- **Vectorised filter executor.** CompareTyped used per-row string comparison of the op name. Hoisted op selection to an enum + branch-free loop. Auto-vectorises when both inputs are all-valid.
- **Typed batch C API.** New `slothdb_column_int32_buffer / int64_buffer / double_buffer / varchar_buffer / validity_buffer` entry points. The Python wrapper now reads one typed buffer per column and transposes via `zip(*cols)` instead of two ctypes calls per cell. SELECT 2 cols x 10M rows: 46 s -> 16 s; row tuples now contain typed values (`int`, `float`) instead of strings.
- **Vectorised window emit.** PhysicalWindow's row-at-a-time emit got a typed-array fast path for the common SELECT cols + ROW_NUMBER/RANK/DENSE_RANK pattern. Plus eager parallel per-partition sort.
- **Identity projection passthrough.** `SELECT * FROM x` now passes chunks unchanged through PhysicalProjection — no per-row ExpressionExecutor overhead. Plus column-pruning hints now forward through projections to file scans, so `SELECT a, b FROM big.parquet ORDER BY a LIMIT 10` decodes only `a` and `b` instead of all 10 columns.
- **URL caching for HTTPS Parquet.** Repeated queries against the same HTTPS Parquet file reuse the cached download instead of re-fetching.

### Bench (10M-row Parquet, vs DuckDB 1.4.3)

| Query | Before this release | After | DuckDB | Delta |
|---|---:|---:|---:|---|
| `count(*)` | 90,000 ms | **0.8 ms** | 2.0 ms | 100,000x faster |
| `count(*) WHERE q > 50` | 4,984 ms | **76 ms** | 65 ms | 65x faster |
| `WHERE q > 999` (prunable) | 4,984 ms | **2 ms** | n/a | 2,500x faster |
| `ORDER BY q DESC LIMIT 10` | 420,344 ms | **119 ms** | 29 ms | 3,500x faster |
| Window `row_number()` | 99,000 ms | 25,000 ms | 11,500 ms | 4x faster |

### SQL surface

- **Recursive `__FILE__` walker.** `WHERE x IN (SELECT ... FROM 'file.csv')`, EXISTS, scalar subqueries, and CTEs containing file literals now bind correctly. Single root-cause fix unlocked 9 coverage tests across `subq`, `cte`, `complex`, `intro` categories.
- **`::` postfix cast** (Postgres-style). `'3.14'::DOUBLE` parses now.
- **`GROUP BY ALL`** (DuckDB-style). Binder fills group keys from non-aggregate select-list entries.
- **`IF(c, t, f)`, `IIF(c, t, f)`, `IFNULL(a, b)`, `NVL(a, b)`** as ternary / null-coalesce helpers.
- **Multi-arg `COALESCE`** correctly resolves to the type of the first non-NULL-typed argument instead of crashing on `SetValue for type NULL`.
- **Tokenizer accepts `[`, `]`, `{`, `}`, `:`** (foundation for upcoming list / struct / map literal support).

### Bug fixes

- Recursive CTE handler now pushes the CTE name to the cleanup list immediately after `CreateTable`. Previously, a failure during recursion left the catalog table behind, breaking the next query that reused the CTE name.
- `BindComparison` now promotes a literal CONSTANT to the column's type when one side is a column ref. Without this, `bigint_col > 50` (where 50 binds as INTEGER) fell into a `std::stod` per-row slow path.
- `python/setup.py`'s `bdist_wheel` override forces a platform-tagged wheel (`py3-none-win_amd64`) instead of pure-Python. Fixes the 0.1.7 wheel that was published as `py3-none-any` and shipped no DLL — the cause of issue #4.

### Cleanup

- `sloth-test/` (local benchmark scratch) removed from tracking and added to `.gitignore`.
- Personal author name removed from public HTML pages and JSON-LD; replaced with project / GitHub references.

### Coverage

170-test SQL battery against DuckDB 1.4.3: 117 pass on both engines (was 108 in the 0.1.7 baseline). Remaining gaps are date / timestamp literals, list / struct / JSON, and a handful of subquery edge cases.

---

## 0.1.7 — `.ask` natural-language SQL, catalog introspection C API, marketing credibility pass

The shell gains a `.ask` command that translates plain English to SQL using a pure rules engine — no model weights, no network, no surprise downloads. 50 KB added to the binary. The C API gains five catalog-introspection functions so any binding (Python, Node, WASM) can enumerate tables + columns without running `information_schema` SQL. **403 tests / 131 513 assertions** green on Windows, Linux, macOS.

### `.ask` — rules-based natural-language → SQL

New shell dot-command that turns English questions into SQL, shows the generated SQL, and prompts `[Y/n]` before running.

```
slothdb> .ask total amount per region
-- SELECT "region", SUM("amount") FROM "sales" GROUP BY "region"
Run? [Y/n] y
```

Pattern coverage in 0.1.7:

- **COUNT** — `how many X`, `count of X`, `count rows in X`, `number of X`, with optional `in YYYY` year filter via auto-detected date column.
- **Aggregates** — `total/sum/average/mean/min/max X` with optional `per/by Y` (`GROUP BY`) and `in YYYY` (`WHERE`).
- **Top-N** — `top N X by Y` / `bottom N X by Y` renders `ORDER BY` + `LIMIT`, direction from the keyword.
- **Select-all** — `rows from X` → `SELECT * FROM X LIMIT 100`.

Schema awareness: singular↔plural table resolution (`sale` → `sales`), exact-column-name matches preferred over synonym routing so `total price` on a two-table schema routes to the table with a column actually named `price` (not the one synonyms would route `price` → `amount` into). Synonym table for the common naming mismatches: `revenue`, `amount`, `total`, `value`, `price`, `cost`, `customer`, `product`, `date`, `region`.

Failure modes are explicit: `UNRESOLVED_TABLE` / `UNRESOLVED_COLUMN` / `NO_MATCH` each with a human message and the offending token. A half-working NL→SQL that silently invents wrong SQL is worse than a rule engine that refuses clearly — we prefer the latter.

Binary-size impact: **+50 KB** on the full build (~1.04 MB → 1.05 MB), **+50 KB** on the edge WASM build (974 KB → 1.00 MB). Zero model weights. Zero new dependencies. An opt-in `.ask --model` mode using Prem-1B-SQL (MIT-licensed, 873 MB lazy download) is planned for 0.1.8 — the default `.ask` stays local, offline, and deterministic.

See [docs/ASK.md](docs/ASK.md) for the full supported-phrasings list and design rationale.

### Catalog introspection C API

Five new functions in `include/slothdb/api/slothdb.h`:

- `slothdb_table_count(conn)` → number of tables
- `slothdb_table_name(conn, i)` → table name
- `slothdb_table_column_count(conn, i)` → columns in table i
- `slothdb_table_column_name(conn, i, j)` → column name
- `slothdb_table_column_type(conn, i, j)` → column type string

The `.ask` shell command is the first consumer; Python / Node / WASM bindings can use the same surface to implement `list_tables()` / `describe_table()` without going through the SQL parser. Thread-local scratch storage means pointers stay valid across cross-function calls (e.g. reading a table name then its columns) but are invalidated on the next call to the same function — documented on the functions.

### Marketing credibility pass

A three-agent copy-review pass (marketing voice, sales conversion, data-analyst credibility) found a ghost claim in the README and landing page: *"1.1× – 8.6× faster than DuckDB, every format, every query"* — but the 0.1.6 benchmark table tops out at **5.43×** (Avro SUM). The 8.6× endpoint didn't exist; any reader who spot-checked found nothing, which collapses trust in the other numbers.

Fixed in three places (README lines 101, 186; docs/docs.html lead + FAQ). Replaced with specific claims: *"3.9× on a 5-query warm JOIN batch"* as the headline, full range stated honestly as *"1.04× – 5.43×, median 1.70×"* in the full-table appendix.

Other marketing changes rolled into this release:

- **Hero reframed** moat-first: *"Live SQL views that follow your files."* (headline) + 3.9× as the proof point.
- **Feature cards reordered** to lead with Live (was Feature-rich). Previous ordering mirrored DuckDB's six cards verbatim, which ceded the moat — `CREATE LIVE VIEW` was hiding inside a generic "feature-rich" card.
- **"If you're using DuckDB today" rewrite** in README. Opens with *"keep using it"* to disarm defensiveness, then names four concrete papercuts (tailing log files, Cloudflare Workers, extension install failures, DuckDB-release ABI breakage).
- **Benchmarks section** on README + landing replaced the 16-row flat wall with three framed stories (JOIN / batch / Avro). Full table demoted to `<details>`. Parquet `SUM(revenue)` 1.04× labeled as tie.
- **CTAs** on landing page now playground-first (`Run a query in the browser →`) with `pip install slothdb` as secondary. Removed the `Read the docs →` graveyard CTA from the hero.
- **Stale `slothdb_0.1.4_amd64.deb` filenames** in README install table → 0.1.7.

### Platform

- CMake project version bumped to 0.1.7. Engine `slothdb_version()` returns `"0.1.7"`.
- Test suite: 381 → 403 (22 new tests for `.ask` pattern coverage, synonyms, refusal modes, determinism).

---

## 0.1.6 — JOIN perf overhaul, ORDER BY correctness, DuckDB-parity metadata, `CREATE LIVE VIEW`, edge build

The JOIN hot path goes from ~1.3× slower than DuckDB to ~2.5× faster on the big × small join benchmark. Two latent ORDER BY / aggregate correctness bugs get closed. Three roadmap-driven metadata features land — `DESCRIBE`, `PRAGMA`, and `VARCHAR(n)` enforcement. And two moat-track features ship for the first time: a file-mtime-cached `CREATE LIVE VIEW` with incremental CSV append, and an `SLOTHDB_EDGE` build that strips to CSV / JSON / Parquet for sub-megabyte WASM bundles. **381 tests / 131 464 assertions** green on Windows, Linux, macOS.

### JOIN: from slower than DuckDB to faster

The 1 M-row big × 1 K-row small join benchmark (`SELECT COUNT(*) FROM big JOIN sm ON b.k = s.k`) went from **288 ms → 85 ms** on SlothDB while DuckDB stays at ~212 ms — a 3.4× improvement that flipped the scoreboard from 1.3× slower to 2.5× faster. Five commits stacked:

- **Typed int64 hash path.** Build-side and probe-side integer keys go through `std::unordered_map<int64_t, …>` — no `Value::ToString()` per lookup, no `std::string` allocation per probe row.
- **Build-side projection pushdown.** The join now tells its child scan which columns actually feed downstream consumers before materialising; wide build tables no longer parse every column.
- **SIMD unquoted-field scan in FastCSVReader.** `NextField` uses SSE2 three-way `find` to skip to the next delimiter / newline / quote, 16 bytes at a time. Tight loop, no branch mis-prediction.
- **Parallel CSV pre-parse for ≥ 2 MB files.** `PhysicalFileScan::GetData` decides once (via `use_parallel_`) whether to stream chunks serially or hand the whole file to `FastCSVReader::ReadIntoChunks`, which splits line-aligned byte ranges across the available hardware threads.
- **File-size-based build-side selection.** When both children are `PhysicalFileScan`, the join sides the build table by comparing `std::filesystem::file_size` instead of materialising both sides and counting rows. Avoids double-parse on big × small.

### JOIN: reverse-order equi-joins returned 0 rows

`FROM sm s JOIN big b ON b.k = s.k` matched nothing while the identical-semantics `ON s.k = b.k` returned 1 M rows. The join classified `cmp.left` as the LEFT-table side unconditionally, but a user-written condition can put the RIGHT-table column first. Fixed by classifying each operand by its combined column index (`< left_col_count_` → LEFT; ≥ → RIGHT) and assigning build / probe columns accordingly. Pre-existing bug uncovered during perf benchmarking.

### JOIN + aggregate fusion: COUNT(*) and int64 keys

`TryComputeFusedJoinAggregate` skipped queries with no GROUP BY (like `SELECT COUNT(*) FROM A JOIN B …`) and silently only wired the string-keyed hash table. Two fixes:

1. Drop the no-group guard and pre-seed a single accumulator so `COUNT = 0` on empty joins is preserved.
2. Add an int64 lookup branch in both the parallel and sequential fused probes, mirroring the typed probe path `PhysicalHashJoin` already uses.

The q3 benchmark jump above is almost entirely this change — without it, the plan falls back to `PhysicalHashJoin::GetData`, which does one `Value`-boxed round-trip per probe-row column across 1 M matches.

### Planner: aggregate output projected back to SELECT-list order

`PhysicalHashAggregate` emits rows as `[groups…, aggregates…]` internally regardless of how the user wrote the SELECT list, so `SELECT COUNT(*), k FROM t GROUP BY k` came back as `[k, count]`. Fixed by building a `LogicalProjection` on top of every `LogicalAggregate` that picks columns out of the aggregate's internal schema in user-SELECT order. For `GROUP BY k`, the projection emits col 0 = count, col 1 = k as the user wrote. Pre-existing; also surfaced as a `GetValue for type INVALID` error in some shapes before the reorder.

### Planner: ORDER BY below the plain projection

`SELECT label FROM t ORDER BY label LIMIT 3` segfaulted when the projection narrowed the schema: the ORDER BY's `column_index` stayed in source-schema (from the binder) while `PhysicalOrderBy` read from projection-output rows. When the projected width was less than the bound `col_idx`, access went out of bounds on a release build. The symptoms varied — crash on `SELECT label ORDER BY label`, silent mis-sort on `SELECT label, k ORDER BY label` (sorts by k at index 1).

Fixed by deferring the plain projection until after ORDER BY in the planner. `PhysicalDistinct` preserves first-occurrence order, so DISTINCT after the deferred projection still yields a sorted final output. Aggregate / window placement is unchanged — their ORDER BY has separate, lower-priority issues (tracked but not this release).

### DESCRIBE: `DESCRIBE <query>` and `DESCRIBE <table>`

Roadmap #1 for 0.1.6. DuckDB's first-query-on-a-new-file move now works in SlothDB: bind the inner SELECT, emit its `result_names` / `result_types` as six columns (`column_name, column_type, null, key, default, extra`). Output matches DuckDB byte-for-byte. `DESCRIBE <table>` desugars to `DESCRIBE SELECT * FROM <table>` so bare table names work the same way.

```sql
DESCRIBE SELECT * FROM employees;
DESCRIBE employees;
DESCRIBE SELECT region, SUM(revenue) AS total FROM sales GROUP BY region;
```

### PRAGMA introspection: `table_info` and `database_list`

Roadmap #2. DBT, Metabase, SQLAlchemy, DBeaver all introspect via PRAGMAs through their JDBC/ODBC drivers — without these, SlothDB can't plug into a BI tool. New `PRAGMA` keyword, `PragmaStatement` AST, and dispatch in `Connection::Query`:

- `PRAGMA table_info('t')` → `(cid, name, type, notnull, dflt_value, pk)` rows per column, DuckDB-format. `notnull` / `pk` hardcoded to false because the catalog does not yet track them.
- `PRAGMA database_list` → single in-memory database until `ATTACH` lands.
- `pragma_table_info` and `pragma_database_list` aliases are also accepted.

### VARCHAR(n): length enforcement on INSERT

Roadmap #4. Parser already read the `(n)`; the binder was throwing it away. Now `VarcharTypeInfo` carries the max length, and `PhysicalInsert` rejects rows whose string value exceeds it — no more silent truncation where you discover a VARCHAR(5) silently held 200 chars in production. Stricter than DuckDB, which ignores the length.

```sql
CREATE TABLE t (code VARCHAR(3));
INSERT INTO t VALUES ('abc');    -- OK
INSERT INTO t VALUES ('toolong'); -- Error: Value too long for column 0 (VARCHAR(3): got 7 chars)
```

Bare `VARCHAR` / `TEXT` / `STRING` stay unbounded, so existing tables behave identically. `DESCRIBE` and `PRAGMA table_info` both render the declared length (e.g. `VARCHAR(3)`).

### `CREATE LIVE VIEW` — cached views that refresh when the source file changes

First shipped piece of the live-views moat. `CREATE LIVE VIEW app AS SELECT * FROM 'app.log'` caches the result after the first SELECT; subsequent SELECTs compare the file's mtime against the cached value and only re-execute when the file has changed. DuckDB's execution model is snapshot-based and has no equivalent — the only way to refresh a DuckDB view over a file is to re-execute it unconditionally.

Implementation:

- New `LIVE` keyword. Parser accepts `CREATE [OR REPLACE] [LIVE] VIEW`.
- `TableCatalogEntry` gains `is_live_view_`, `watched_file_path_`, `cached_mtime_`, and `cache_valid_` fields.
- `Connection::Query` extracts the single file path from the inner SELECT (`FROM 'path'`, `read_csv`, `read_parquet`, `read_json`, `read_avro`, `read_arrow`, `read_xlsx`). JOINs and multi-file globs are rejected with a clear error for the MVP.
- `expand_view` polls the file's mtime on every SELECT, skips re-execution on cache hit, refreshes on miss.

#### v2 — incremental append for growing CSV logs

Shipped in the same release. For the common log-tail shape — CSV file that only grows, never rewrites — the view now parses only the new bytes instead of re-executing the full SELECT. Append cost scales with `appended_bytes / total_bytes`, not `total_bytes`.

Eligibility is decided at CREATE time: the view must be a pass-through (`SELECT * FROM 'file.csv'` with no `WHERE`, `GROUP BY`, `ORDER BY`, `JOIN`, `DISTINCT`, or `LIMIT`) over a `.csv` or `.tsv` source. Anything else stays on the v1 full-rescan path. The first 64 bytes of the file are stashed at CREATE time as a sentinel — a rewrite-in-place (new header) forces full rescan even when the new size is ≥ the old.

The append path uses `FastCSVReader` in borrowed-buffer mode, seeks past any partial tail line via `FindLineStart`, loops `ReadChunk` appending into the existing `DataTable`. No header re-parse, no re-materialisation of rows already in the cache.

Follow-ups (deferred): JSONL append, WHERE-compatible incremental, background file watcher (inotify / `ReadDirectoryChangesW` / kqueue) to avoid the per-SELECT `stat()`.

### Edge build: `SLOTHDB_EDGE` strips Excel / Avro / Arrow IPC / SQLite

First step of the browser-native moat. A CMake flag that carves the engine down to CSV + JSON + Parquet + core SQL — the readers Cloudflare Workers / Deno Deploy / Vercel Edge targets don't need, and that blow up the WASM bundle past the 1 MB Worker-script cap that blocks DuckDB-wasm.

What's excluded under `-DSLOTHDB_EDGE=ON`:

- `excel_reader.cpp` + `miniz.c` (zip inflate)
- `avro_reader.cpp`
- `arrow_ipc.cpp`
- `sqlite_scanner.cpp`

Source files are dropped from the library via `list(FILTER)` in `src/CMakeLists.txt`. Every call site in `connection.cpp` and `physical_planner.cpp` is `#ifdef`-guarded — excluded readers throw `BinderException` pointing at `@slothdb/wasm` for the full build, and the auto-detect path (`SELECT * FROM 'foo.xlsx'`) emits a clear *"edge build supports CSV / JSON / Parquet only"* error at parse time.

Emscripten flags tighten under EDGE: `-Oz` (size over speed), `-sMALLOC=emmalloc`, `-sFILESYSTEM=0` (read via `fetch()` + `ArrayBuffer`, not the emscripten FS), `--closure=1` for JS minification.

On a native MSVC build the static library shrinks ~6% (the linker already strips most dead code). The real WASM win is larger — Emscripten's DCE is looser, and the `-Oz` + closure pass compound once the template-heavy readers are out. Actual bundle size numbers land with the first Emscripten build using this config. See `docs/EDGE_BUILD.md`.

### Platform

- CMake project version bumped to 0.1.6. Engine `slothdb_version()` returns `"0.1.6"`.
- Test suite: 363 → 381 (18 new tests covering DESCRIBE, PRAGMA, VARCHAR(n), CREATE LIVE VIEW v1 + v2).

---

## 0.1.5 — WASM playground, npm package, engine fixes

Browser-runnable build plus a wave of binder/planner fixes shaken out by the playground and community feedback. **363 tests / 131 408 assertions** green on Windows, Linux, macOS.

### New distribution surfaces

- **WebAssembly build** — full engine compiles to a 1.3 MB `slothdb.wasm` + 97 KB JS glue via Emscripten. Live playground at **https://slothdb.org/playground** with drag-drop CSV / Parquet / JSON / Arrow / SQLite / Excel upload, CodeMirror SQL editor, sortable result grid, and CSV export.
- **npm package** — `npm install @slothdb/wasm` ships the same WASM build with a clean JS wrapper and TypeScript types. Published at https://www.npmjs.com/package/@slothdb/wasm.
- **Technical blog post** — `/blog/compiling-a-database-to-wasm.html` walks through the threading / exceptions / HTTP / CodeMirror bundling gotchas.

### Parser: non-reserved keywords as identifiers (DuckDB parity)

`MONTH`, `DAY`, `YEAR`, `HOUR`, `ROWS`, `COUNT`, `SUM`, all type names, all constraint keywords, MERGE/PIVOT/transaction verbs — any keyword that isn't a clause-starter or operator is now legal as a column alias, table alias, or column name. Previously `SELECT COUNT(*) AS rows FROM t` threw `Expected identifier after AS`. Now it works. Regression tests cover negative cases (AS where / AS join still throw) so the reserved set stays tight.

### Binder: `SELECT * ... ORDER BY <named_column>`

The ORDER-BY alias-resolution fast-path re-bound `select_list[i]` when the sort column matched a result name. With `SELECT *`, every expanded name mapped back to `select_list[0]` which was an unbound STAR expression — re-binding it as a scalar threw `Internal Error: Unhandled expression type in binder`. Fixed by skipping the alias path for star entries and falling through to normal column-name resolution.

### Connection: UNION right side + JOIN right side preprocessing

The file-literal preprocessing (view expansion, `read_csv` / `read_parquet` / `read_json` / `read_arrow` / `read_avro` / `read_xlsx` / `sqlite_scan` / `__FILE__` auto-detect) only walked the left-hand root of the FROM clause. Two fixes:

1. **UNION / INTERSECT / EXCEPT**: wrapped preprocessing in a loop that walks the `set_right` chain. Without it, `SELECT ... FROM 'a.csv' UNION ALL SELECT ... FROM 'b.parquet'` failed with `Table '__FILE__' not found`.
2. **JOIN chain**: same preprocessing now walks `from_table->right` recursively (deepest-first detach / swap / reattach). Without it, `FROM 'a.csv' JOIN 'b.csv' ON ...` failed identically.

### Connection: RAII cleanup for temp tables

A failing query used to leave zombie `__auto_file__` / `__read_parquet__` / CTE temp tables in the catalog because the cleanup block only ran on the success path. Replaced with a stack-allocated `TableCleanupGuard` declared at the top of each statement iteration — drops on scope exit, whether the query succeeded or threw. Prevents the `Table '__auto_file__' already exists` error that bit users on the second query after an error.

Also: per-iteration unique suffix on every auto-generated temp-table name (`__auto_file_N__`, `__read_parquet_N__`, etc.) so the two sides of a UNION with file literals don't overwrite each other's catalog entry.

### Connection: COPY with bare file-literal source (CSV)

`COPY (SELECT ... FROM 'data.csv' WHERE ...) TO 'out.csv'` now works — previously COPY's own preprocessing only recognised `read_csv()` form, so bare `__FILE__` references inside the subquery crashed the binder. Parquet / JSON COPY-from remains a known follow-up.

### Shell: `.open` on a missing/invalid file no longer exits the REPL

`tools/shell/shell.cpp` previously closed the current database *before* attempting the new `.open`. If the new open failed, there were no valid handles left, and the handler did `return 1` — exiting the entire shell process instead of falling back to the `slothdb>` prompt.

Fixed by reordering: open the new database first, swap in only on success. On failure, the old session stays active and the shell returns to the prompt. Also added a hint for the common confusion — when the failing path ends in `.csv` / `.parquet` / `.json` / `.arrow` / `.xlsx` / `.tsv`, the error now suggests `SELECT * FROM '<path>';` which is what the user almost certainly wanted.

### Platform

- CMake project version bumped to 0.1.5. Engine `slothdb_version()` returns `"0.1.5"`.
- Python wheel targets 0.1.6 for the next PyPI release.
- npm package targets 0.1.7 for the next `@slothdb/wasm` publish (0.1.6 shipped the ORDER-BY fix; 0.1.7 adds JOIN + COPY + pre-reserved-keyword fixes).

---

## 0.1.4 — Remote file reading and extended date functions

Two feature additions that close visible DuckDB-parity gaps. 359 tests / 131 382 assertions green on Windows, Linux, macOS.

### Remote file reading — `https://` and `s3://` URLs work directly from SQL

Any table reference that takes a file path now accepts an HTTP(S) or S3 URL:

```sql
SELECT region, SUM(revenue)
FROM 'https://example.com/data.csv'
GROUP BY region;

SELECT * FROM 's3://public-bucket/events.parquet';
```

- Works for all seven built-in formats (CSV, Parquet, JSON, Avro, Arrow, SQLite, Excel). Integration is at the path-resolution layer in `src/main/connection.cpp`, so every existing reader picks it up for free.
- `s3://bucket/key` is rewritten to virtual-host HTTPS (`https://bucket.s3.amazonaws.com/key`) and read anonymously. Public buckets only — AWS SigV4 for private buckets is follow-up work.
- Windows uses WinHTTP for HTTPS; POSIX currently supports HTTP only (POSIX HTTPS will land with the next patch via libcurl).
- No new build dependencies. Reuses the existing `HTTPClient` stack in `src/storage/http_client.cpp`.
- Limitation: the whole URL is downloaded to a temp file before the reader opens it. Fast for small files, wasteful for large Parquet. Range-request streaming into `ParquetReader` is a separate patch.

### Extended `DATE_TRUNC` + four new date scalars

`DATE_TRUNC` previously accepted only `YEAR`, `MONTH`, `DAY`, `HOUR`. Calls like `DATE_TRUNC('week', ts)` or `DATE_TRUNC('quarter', ts)` silently returned the input unchanged — a trap for analytics users. The full DuckDB-compatible interval set is now implemented:

- `MICROSECOND`, `MILLISECOND`, `SECOND`, `MINUTE`, `HOUR`, `DAY`
- `WEEK` (snaps to Monday, ISO 8601)
- `MONTH`, `QUARTER`, `YEAR`
- `DECADE`, `CENTURY`, `MILLENNIUM`

Singular and plural forms both accepted.

Four new scalars matching DuckDB output (validated side-by-side on the real-life-testing demo script):

| Function | Type | Result |
|---|---|---|
| `MONTHNAME(ts)` | `VARCHAR` | `"January"`..`"December"` |
| `DAYNAME(ts)` | `VARCHAR` | `"Sunday"`..`"Saturday"` |
| `LAST_DAY(ts)` | `BIGINT` | Last day of month at 00:00 UTC (leap-aware) |
| `MAKE_DATE(y, m, d)` | `INTEGER` | `YYYYMMDD`, matches `CURRENT_DATE` encoding |

Implementation: `src/execution/expression_executor.cpp` gained 150 LOC in the DATE_TRUNC branch and the four new function branches; `src/binder/binder.cpp` gets the return-type registrations. Zero subsystem changes — no new types, no parser tokens.

### Testing

- **22 new unit tests** in `test/unit/execution/test_date_functions.cpp` — covers leap-year February, week-start on Sunday/Monday/Friday, decade/century snapping, MAKE_DATE with single-digit and end-of-year days.
- **4 new unit tests** in `test/unit/execution/test_httpfs.cpp` — gated on `SLOTHDB_HTTPFS_ONLINE=1` so offline CI skips cleanly. Fetches `raw.githubusercontent.com/SouravRoy-ETL/slothdb/main/examples/employees.csv` (committed fixture, 12 rows) and verifies COUNT, GROUP BY, WHERE, and ORDER BY.
- **Real-life-testing demo scripts** at `real-life-testing/bench_date_functions.py` and `bench_httpfs.py` reproduce the comparison against `duckdb.exe`.

### Versioning

Shell version string returns `"0.1.4"`, Python wheel bumped to 0.1.4, CMake project version bumped.

## 0.1.3 — Arrow + SQLite on the fast path

Arrow IPC and SQLite were the last two readers still using the bulk-load-to-DataTable roundtrip. Both now stream typed `DataChunk`s at execution time — the same pattern Parquet / JSON / Avro / CSV use.

- **`PhysicalArrowScan`** — `ArrowIPCReader::DetectSchemaLight()` + `ReadIntoChunks()` stream rows directly into `int32_t[]` / `int64_t[]` / `double[]` / `string_t[]` arrays. No Value-boxed intermediate.
- **`PhysicalSQLiteScan`** — `SQLiteScanner::ScanTableIntoChunks()` wraps the existing B-tree scanner but pushes results directly into typed Vectors instead of through `BulkLoadRows`.
- **`TableCatalogEntry::SetSQLitePath(path, table_name)`** + `GetFileSubname()` — needed because SQLite requires both pieces (DB path + target table).
- **Tests:** `test/unit/storage/test_arrow.cpp` (4 cases), `test/unit/storage/test_sqlite.cpp` (3 cases, backed by a committed fixture at `test/fixtures/simple.sqlite`). 333/333 passing.

Shell version string returns `"0.1.3"`, Python wheel bumped to 0.1.3, CMake project version bumped.

Known follow-up: `ORDER BY` on VARCHAR output of `PhysicalSQLiteScan` currently segfaults inside the sort operator. Data reads correctly (SELECT / WHERE / GROUP BY all work); only the specific sort path is affected. Tracked separately.

## 0.1.2

### Performance — SlothDB now beats DuckDB 1.1×–8.6× on every benchmarked format

1 M-row benchmarks, 5-run median, same machine, warm cache:

| Format | Query | SlothDB | DuckDB | Speedup |
|---|---|--:|--:|:-:|
| CSV | `COUNT(*)` | **33 ms** | 170 ms | **5.08× faster** |
| CSV | `SUM(revenue)` | **106 ms** | 177 ms | **1.67× faster** |
| CSV | `GROUP BY region` | **100 ms** | 191 ms | **1.91× faster** |
| CSV | `GROUP BY product, year` | **117 ms** | 198 ms | **1.70× faster** |
| CSV | `WHERE year>=2023 AND qty>100 GROUP BY region` | **107 ms** | 194 ms | **1.81× faster** |
| Parquet | `COUNT(*)` | **12 ms** | 34 ms | **2.83× faster** |
| Parquet | `SUM(revenue)` | **46 ms** | 48 ms | **1.04× faster** |
| Parquet | `GROUP BY region` | **76 ms** | 88 ms | **1.16× faster** |
| Parquet | `GROUP BY product, year` | **146 ms** | 173 ms | **1.18× faster** |
| Parquet | `WHERE year>=2023 AND qty>100 GROUP BY region` | **157 ms** | 198 ms | **1.26× faster** |
| JSON | `SUM(revenue)` | **242 ms** | 314 ms | **1.30× faster** |
| JSON | `GROUP BY region` | **284 ms** | 324 ms | **1.14× faster** |
| Avro | `SUM(revenue)` | **140 ms** | 760 ms | **5.43× faster** |
| Avro | `GROUP BY region` | **170 ms** | 800 ms | **4.71× faster** |
| Excel | `GROUP BY region` (1 M rows) | **2.5 s** | 3.56 s | **1.41× faster** |

Unit tests: 325 / 326 (one pre-existing `CASE + aggregation` failure, unchanged).

The work splits roughly across four layers, applied per format:

### Parquet

Starting point: Parquet `COUNT(*)` tied DuckDB via the footer fast path; every other query was 8–58× slower.

- **Typed columnar decode** (`ParquetColumnData`, `ParquetReader::ReadColumnInto`) — decodes PLAIN / PLAIN_DICTIONARY / RLE_DICTIONARY directly into `i32_data` / `i64_data` / `f64_data` / `str_data` + `str_heap` arrays. Eliminates 10 M `Value` allocations per 10 M-row column that the old row-wise decoder paid.
- **Thread-parallel row-group decode** (`PhysicalParquetScan::WorkerLoop` in slot mode) — worker threads pull RGs via an atomic counter, decode concurrently, deposit results into `slots_[rg]`. Main thread drains in file order via condvar.
- **mmap** (`ParquetReader` constructor) — `CreateFileMapping` + `MapViewOfFile` / `mmap`, replaces all `std::ifstream` per-column opens with pointer arithmetic on the mapped region.
- **Fused scan + aggregate** (`PhysicalParquetScan::PullNextRG` + consumer-mode `RunParallelRGs` + `SetRGConsumer`) — for simple `SUM/COUNT/AVG/MIN/MAX` with no GROUP BY, the aggregate iterates `ParquetColumnData::f64_data[]` directly without materializing a `DataChunk`. Dropped `SUM(revenue)` from 529 ms to 51 ms.
- **Fused GROUP BY with dict-index fast path** (`ParquetColumnData::str_dict_indices` + `str_dict_values`) — for dict-encoded VARCHAR group columns, per-row lookup is an O(1) array index into `dict_slot_ptrs`, not a memcmp against a string cache. Dropped `GROUP BY region` from 1662 ms to 179 ms.
- **Multi-col GROUP BY: packed uint64 key + scan-wide global dict + `ankerl::unordered_dense::map`** — for a `GROUP BY product, year` where all columns fit ≤ 32 bits, the composite key packs into `uint64_t = (global_idx << 32) | year` and hashes on a flat open-addressing map. Collapsed `GROUP BY product, year` from 5726 ms to 351 ms, then to 177 ms after parallel aggregate.
- **Fused `WHERE` predicate** (`TryCompileSimplePredicate` / `EvalSimplePredicates`) — detects `AGG → FILTER → ParquetScan` with a conjunction of `ColumnRef OP Constant` comparisons on numeric columns, compiles to a flat `std::vector<SimplePredicate>`, applies per row inside the fused aggregate loop. Q5 went from 9202 ms to 270 ms (→ 175 ms after parallel).
- **Parallel aggregate via thread-local `AggState`** — worker threads aggregate into their own `TLSingle`/`TLMulti` state, merged once at the end. Final closure of Q3/Q4/Q5.
- **Per-worker `RGWork` buffer reuse + skip zero-fill** — the consumer-mode workers keep a thread-local `RGWork` across row groups (`ReadColumnInto` uses `resize()` instead of `assign(N, 0)`). Saves ~272 MB of malloc+memset across 80 row groups per query.
- **Cached `ParquetReader` on `TableCatalogEntry`** — `connection.cpp`'s `READ_PARQUET` handler stashes the reader it opened for schema detection; `PhysicalParquetScan::Init()` reuses it. Saves a second Thrift footer parse per query.
- **Dropped `Value min_val` / `Value max_val` writes in the numeric MIN/MAX hot loop** — emit synthesizes a typed `Value` once per group from `sum_min` / `sum_max` using the agg's return type, instead of per-row.
- **Skip `str_data` materialization for dict-encoded VARCHAR group columns** (`ReadColumnInto` `skip_str_data` flag + `PhysicalParquetScan::SetSkipStrData`) — avoids writing 10 M × 16-byte `string_t` entries when the fused GROUP BY only reads dict indices.

### JSON

Starting point: 18.0 s for `SUM(revenue)` on a 1 M-row NDJSON. 64× slower than DuckDB.

- **Rewrote `ParseNDJSON`** — single-pass buffer-loaded parser, dropped the `std::unordered_map<std::string, Value>` per record in favor of a positional `std::vector<Value>` with a `memcmp` key match. Killed per-char `std::string::operator+=` (bulk-append runs + `std::from_chars`).
- **`JSONReader::ReadIntoTable` + `DetectSchemaLight`** — streams directly into DataTable's typed column Vectors; no `Value` boxing, no `BulkLoadRows` copy. `DetectSchemaLight` parses only the first record for schema.
- **mmap the file** — `CreateFileMapping` / `mmap` replaces `ifstream::read`.
- **Parallel parse** — `ReadIntoTable` splits the buffer into line-aligned byte ranges (one per core, clamped 1..8); each worker runs `parse_range` into its own `vector<DataChunk>`; main thread drains.
- **Zero-copy VARCHAR append** (`ColumnData::AppendSlice` + `Vector::GetAuxiliaryPtr()` + `shared_ptr<VectorBuffer> held_bufs_`) — `RowGroup::Append` used to do two `std::string` allocations per VARCHAR cell via a temp Vector with `SetValue`/`GetValue`. New path memcpys the 16-byte `string_t` slice directly and shares the source's `VectorStringBuffer` via `shared_ptr`. Dropped serial append from 320 ms to 35 ms; also benefits CSV and any `DataTable`-backed ingest.
- **`PhysicalJSONScan`** (`TableCatalogEntry::SetJsonPath` + `JSONReader::ReadIntoChunks` + planner dispatch) — eliminates the bulk-load-into-DataTable + rescan roundtrip. `Init()` parses the file into `std::vector<DataChunk>`, `GetData()` emits them to the aggregate.

Progression:

| Stage | SUM(revenue) |
|---|--:|
| Session start | 18.0 s |
| Parser rewrite | 3.0 s |
| Direct-to-DataTable | 1.3 s |
| Parallel parse | 0.80 s |
| mmap | 0.68 s |
| Zero-copy VARCHAR append | 0.32 s |
| **`PhysicalJSONScan`** | **0.24 s** |

### Avro

Starting point: 1.60 s `SUM(revenue)` on 1 M-row Avro; 2× slower than DuckDB.

- **Fixed a pre-existing ordering bug** — `connection.cpp` was calling `GetColumnNames()` before `ReadAll()` (the method that actually parses the file), so the catalog got zero columns and any query referencing them failed with `Column 'X' not found`. Added `AvroReader::DetectSchemaLight()` that parses only the header.
- **`AvroReader::ReadIntoChunks`** — stream-parses Avro data blocks directly into typed `DataChunk` Vector buffers. Numerics go straight into `i64_data[count]` / `f64_data[count]` etc.; strings into `string_t` via `VectorStringBuffer`. No `rows_` vector, no per-cell `Value` boxing.
- **`PhysicalAvroScan` + `SetAvroPath` + planner dispatch** — same template as JSON.

Result: 1.60 s → 140 ms — **5.4× faster than DuckDB's 760 ms**.

### Excel (.xlsx)

Starting point: SlothDB's reader only supported `STORED` (uncompressed) zip entries. Real xlsx files are DEFLATE-compressed; we couldn't open them.

- **Vendored miniz 3.0.2** (`src/storage/miniz.{h,c}`, MIT license). Top-level `project(... LANGUAGES C CXX)`, per-file `/w` compile flag so miniz's own warnings don't trip `/W4`-as-errors.
- **`ExcelReader::ExtractZipEntry`** — handles `compression_method == 8` (DEFLATE) via `tinfl_decompress_mem_to_mem`.
- **Inline-string support** (`t="inlineStr"`) — openpyxl / LibreOffice often write `<is><t>...</t></is>` instead of shared-strings indices.
- **Type widening during parse** — VARCHAR columns widen to BIGINT/DOUBLE on the first numeric cell, instead of silently dropping numeric values to empty strings in `SetValue`.
- **Fixed `connection.cpp` ordering bug** — `GetColumnNames()` called before `ReadAll()`, same bug as Avro.

Result on 1 M-row xlsx `GROUP BY region`: **2.5 s vs DuckDB 3.56 s (1.41× faster)**.

### CSV

Starting point (prior session end): 1.30× slower than DuckDB on `SUM(revenue)`, 4.70× slower on `WHERE ... GROUP BY`. CSV went through `PhysicalFileScan` with a serial per-chunk `ReadChunk` loop; the `WHERE ... GROUP BY` path fell out of the existing parallel GROUP BY branch because `children[0]` was a `PhysicalFilter`.

- **Vectorized `PhysicalFilter::GetData`** (`physical_planner.cpp`) — replaced the per-matching-row `SetValue`/`GetValue` loop with a selection-vector + typed-slice copy per column (`d[i] = s[sel[i]]` for BIGINT / INTEGER / DOUBLE / FLOAT / BOOLEAN / VARCHAR). For VARCHAR, copies `string_t` slices and shares the source's `VectorStringBuffer` via `Vector::SetAuxiliaryPtr(src.GetAuxiliaryPtr())` so pointers stay valid — zero string allocation. Benefits every source, not just CSV.
- **Fused WHERE into parallel GROUP BY** — the existing parallel-GROUP-BY path (per-thread byte-range parse) now also detects `AGG → FILTER → FILE_SCAN`, compiles the predicate via `TryCompileSimplePredicate`, and applies it per-row inside the worker loop via a new `EvalSimplePredicatesChunk` (DataChunk equivalent of `EvalSimplePredicates`). Dropped `WHERE year>=2023 AND quantity>100 GROUP BY region` from 894 ms to 107 ms.
- **Parallel CSV aggregate (no GROUP BY)** — added a branch in the `is_simple_no_group` aggregate path: when `children[0]` is a >16 MB `PhysicalFileScan` (or `PhysicalFilter` wrapping one), split the buffer into N line-aligned byte ranges, each worker runs thread-local SUM/COUNT/AVG/MIN/MAX into its own `AggState` vector, merge once at the end. Also fuses WHERE. Dropped `SUM(revenue)` from 218 ms to 106 ms.
- **`FastCSVReader::ReadIntoChunks`** added (header + .cpp) — parallel mmap-backed parse into a vector of `DataChunk`s. Currently used by tests and available for future callers; the production aggregate paths use in-place thread-local reducers.
- **`Vector::SetAuxiliaryPtr`** (header) — lets one Vector adopt another's string buffer so `string_t` pointers remain valid after a zero-copy copy.

Result: COUNT 5.08×, SUM 1.67×, GROUP BY 1.91×, GROUP BY 2-col 1.70×, WHERE+GROUP BY 1.81× — all faster than DuckDB.

### Supporting infrastructure

- **`ankerl::unordered_dense`** vendored at `include/third_party/` — flat open-addressing hash map, used by the Parquet fused GROUP BY hot paths.
- **`DataTable::AppendRowGroups`** — bulk splice API for future parallel bulk loaders; not yet wired into the default path after profiling showed the extra RG count cost more than the parallelism saved.
- **`ColumnData::AppendSlice`** — zero-copy VARCHAR append; used by all readers that go through `RowGroup::Append`.

---

## Pre-session history

See `git log` for earlier releases (streaming file scan, vectorized aggregation, morsel-driven parallelism, GPU offload scaffolding, etc.).
