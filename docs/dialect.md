# The SQL dialect of libzugodb

libzugodb accepts a documented subset of DuckDB's SQL, as DuckDB v2.0.0
(the tag in `test/conformance/duckdb-tag`) answers it. The rule, for every
statement ([design §3](../.agents/design.md#3-dialect)):

1. If the engine accepts it, the result is DuckDB's, value for value and
   type for type, or the difference is listed below with its reason.
2. Otherwise it is an error. An error is never a defect by itself.

How much of DuckDB's own test suite the engine passes, refuses or answers
otherwise is measured by the conformance subset (design §5) and committed
in `test/conformance/<platform>/totals.tsv`. A form below is accepted;
whether a given use of it is right is what those counts measure.

## Statements

| Statement | Notes |
| --- | --- |
| `SELECT` | `DISTINCT`, `FROM` with tables, views, subqueries and `VALUES`, `WHERE`, `GROUP BY`, `HAVING`, `QUALIFY`, `ORDER BY` with `ASC`/`DESC` and `NULLS FIRST`/`LAST`, `LIMIT`, `OFFSET`, window functions with `OVER (PARTITION BY ... ORDER BY ...)` |
| `WITH`, `WITH RECURSIVE` | a CTE's column alias list names its columns |
| `UNION [ALL]`, `INTERSECT`, `EXCEPT` | |
| Joins | `[INNER]`, `LEFT`, `RIGHT`, `FULL [OUTER]` with `ON`; `CROSS JOIN` and `FROM a, b` |
| `VALUES` | as a statement and as a table |
| `CREATE TABLE` | `[OR REPLACE]`, `IF NOT EXISTS`, `NOT NULL`, `PRIMARY KEY` on a column; `AS SELECT` |
| `CREATE [OR REPLACE] VIEW` | |
| `DROP TABLE`, `DROP VIEW` | `IF EXISTS` |
| `INSERT INTO` | a column list, `VALUES` or a query |
| `UPDATE ... SET ... [WHERE]` | |
| `DELETE FROM ... [WHERE]` | |
| `TRUNCATE [TABLE]` | |
| `ALTER TABLE` | `ADD [COLUMN]`, `DROP [COLUMN]`, `RENAME [COLUMN] ... TO` |
| `MERGE INTO ... USING ... ON` | `ON` a column of the target `=` a column of the source, of one type, each named with its table; `WHEN MATCHED THEN UPDATE SET` and `WHEN NOT MATCHED THEN INSERT [(columns)] VALUES` from columns of the source of the target column's type, or `NULL` in `VALUES`; not into a table with a primary key |
| `DESCRIBE` | of a table or a query |
| `SHOW TABLES`, `SHOW DATABASES` | |
| `PRAGMA table_info('t')` | |
| `EXPLAIN` | the plan text is the engine's own, not DuckDB's (design §3, L20) |

Statements end at a semicolon or the end of the text; several may be sent
at once, and the last one's result is returned. Comments are `--` to the
end of the line and `/* ... */`.

### Identifiers

- A name is a letter or `_` followed by letters, digits and `_` (bytes past
  ASCII are letters), or any text between double quotes or backticks.
  Inside quotes, a doubled quote is one quote: `"a""b"` is `a"b`. An
  empty quoted name is an error, as in DuckDB.
- Names match ignoring the case of ASCII letters, quoted or not, and keep
  the spelling they were created with: `SELECT foo FROM t` of a column
  `Foo` returns a column named `Foo`. Two names of a table, of a select
  list in a subquery or view, or two aliases in one `FROM`, that differ
  only in case are the same name: `CREATE TABLE` and `ALTER TABLE` refuse
  it, and a subquery's, a view's and `CREATE TABLE AS`'s later duplicate
  is renamed `name_1`, `name_2`, ... as DuckDB's is.

### Literals

Integers (an `INTEGER` if it fits, else a `BIGINT`), decimals and
exponents (`DOUBLE`; DuckDB's `DECIMAL`, see below), `'strings'` with `''`
for a quote, `TRUE`, `FALSE`, `NULL`, `DATE '...'`, `TIME '...'` and
`TIMESTAMP '...'`. An integer literal beside an integer of another type
takes that type when it fits it, as DuckDB's does: `x + 1` of a `TINYINT`
`x` is `TINYINT` arithmetic.

## Types

| Type | Also spelled |
| --- | --- |
| `BOOLEAN` | `BOOL` |
| `TINYINT` | `INT1` |
| `SMALLINT` | `INT2` |
| `INTEGER` | `INT`, `INT4` |
| `BIGINT` | `INT8` |
| `FLOAT` | `REAL`, `FLOAT4` |
| `DOUBLE` | `FLOAT8` |
| `VARCHAR` | `TEXT`, `STRING`, `CHAR`; a length `(n)` is accepted and has no effect, as in DuckDB (design L13) |
| `BLOB` | `BYTEA` |
| `DATE` | years 1 to 9999 (design L19) |
| `TIME` | |
| `TIMESTAMP` | `DATETIME`, `TIMESTAMP(p)` for `p` of 4 to 6 |

Integer arithmetic is done in the wider operand's type and an overflow is
DuckDB's error (#43). `/` divides as doubles. `FLOAT` arithmetic with a
`FLOAT` or an integer is `FLOAT`, with a `DOUBLE` `DOUBLE`.

### NaN, the infinities and -0.0

As DuckDB's (design L30, #42): a NaN equals every NaN, whatever its sign,
and is greater than every other value, `+inf` included, in comparisons,
`ORDER BY` (after `+inf`, before `NULL` under `NULLS LAST`), `MIN` and
`MAX`; every NaN is one key of `GROUP BY`, `DISTINCT`, a join, a set
operation and `PARTITION BY`. `-0.0` equals `0.0` and is one key with it.
`0 / 0`, `inf - inf`, `sqrt(-1)`, `ln(-1)`, `acos(2)` and a double's
`% 0` are NaN; a NaN cast to an integer is an error (`NULL` under
`TRY_CAST`), to `BOOLEAN` true; `sign` of one is 0. `'nan'`, `'-nan'`,
`'nan(...)'`, `'inf'` and `'infinity'` are read in any case. A `FLOAT` or
`DOUBLE` as text is DuckDB's: `1.0`, `1e+20`, `-0.0`, `inf`, `nan`, and
`-nan` for a NaN whose sign bit is set. That sign, of a NaN arithmetic
makes, is the hardware's, as in DuckDB (`(0.0 / 0.0)::VARCHAR` is `-nan`
on x86-64 and `nan` on arm64); a literal's is fixed. Of equal values,
`MIN` and `MAX` return the last, `GREATEST` and `LEAST` the first, a
group its first row's, as DuckDB's (`GREATEST` and `LEAST` of `FLOAT`s
return `0.0` for `-0.0` and the positive NaN, as DuckDB's do). A table and
a database file hold a NaN with its bits.

## Functions

Every function the engine computes is below; any other name is an error
at bind time, and so is a call with another number or type of arguments
than DuckDB's. `tools/check_dialect.sh` checks that this list is the
binder's registry (`src/binder/binder.cpp`), name for name.

<!-- BEGIN FUNCTIONS (tools/check_dialect.sh) -->

Numbers:
`abs`, `acos`, `acosh`, `asin`, `asinh`, `atan`, `atan2`, `atanh`, `cbrt`,
`ceil`, `ceiling`, `cos`, `cosh`, `degrees`, `exp`, `factorial`, `floor`,
`gcd`, `isfinite`, `isinf`, `isnan`, `lcm`, `ln`, `log`, `log10`, `log2`,
`mod`, `pi`, `pow`, `power`, `radians`, `round`, `sign`, `sin`, `sinh`,
`sqrt`, `tan`, `tanh`, `trunc`.

Text and bytes:
`ascii`, `bit_length`, `chr`, `concat`, `concat_ws`, `contains`,
`ends_with`, `from_base64`, `hex`, `instr`, `left`, `length`, `lower`,
`lpad`, `ltrim`, `md5`, `octet_length`, `position`, `prefix`, `repeat`,
`replace`, `reverse`, `right`, `rpad`, `rtrim`, `sha1`, `sha256`,
`split_part`, `starts_with`, `strlen`, `strpos`, `substr`, `substring`,
`suffix`, `to_base64`, `to_hex`, `translate`, `trim`, `unhex`, `upper`.
`upper` and `lower` of text past ASCII are refused until a Unicode case
table exists (design L22, #39).

Dates and times, of a `DATE` or a `TIMESTAMP`:
`century`, `date_diff`, `datediff`, `date_part`, `date_trunc`, `day`,
`dayname`, `dayofmonth`, `dayofweek`, `dayofyear`, `decade`, `epoch_ms`,
`hour`, `isodow`, `isoyear`, `last_day`, `make_date`, `make_timestamp`,
`millennium`, `minute`, `month`, `monthname`, `quarter`, `second`,
`strftime`, `week`, `year`; and `EXTRACT(part FROM x)`. `date_diff` and
`datediff` take DuckDB's three arguments (part, start, end) and count
the part's boundaries between them, for the parts year, quarter, month,
day, decade, century, millennium, hour, minute, second, millisecond and
microsecond. `epoch_ms` takes a `TIMESTAMP`.

Conditions and NULLs:
`coalesce`, `greatest`, `if`, `ifnull`, `least`, `nullif`; and `CASE`.

The session:
`current_schema`.

Aggregates:
`avg`, `bool_and`, `bool_or`, `count`, `group_concat`, `listagg`, `max`,
`median`, `min`, `stddev`, `stddev_pop`, `stddev_samp`, `string_agg`,
`sum`, `var_pop`, `var_samp`, `variance`.

Window functions:
`dense_rank`, `first_value`, `lag`, `last_value`, `lead`, `ntile`,
`rank`, `row_number`; and `sum`, `count`, `avg`, `min` and `max` over a
window. The window shapes the engine computes are those of design L21;
any other is refused.

<!-- END FUNCTIONS -->

Operators: `+`, `-`, `*`, `/`, `%`, unary `-`, `^` (`power`), `||`,
`&`, `|`, `<<`, `>>`, `=`, `<>`, `!=`, `<`, `<=`, `>`, `>=`, `AND`,
`OR`, `NOT`, `IS [NOT] NULL`, `IS [NOT] TRUE`/`FALSE`/`UNKNOWN`,
`IS [NOT] DISTINCT FROM`, `[NOT] BETWEEN`, `[NOT] IN`, `[NOT] LIKE`,
`[NOT] ILIKE` (ASCII patterns, design L22), `ESCAPE`, `EXISTS`, `CAST`,
`TRY_CAST` and `::`.

### Functions a host provides

These need a table the engine's host supplies
(`include/zugodb/main/providers.hpp`, design L25 and L29). Without one
they are refused, with the error of the list below; this repository's
tests and conformance counts run without one.

- **Regular expressions**, with a regex table: `regexp_matches`,
  `regexp_full_match`, `regexp_replace`, `regexp_extract` with a constant
  integer group, and `[NOT] SIMILAR TO` (DuckDB's `regexp_full_match`).
  The options are a constant string of `c` and `i` (the last wins), and
  `g` for `regexp_replace`; any other letter is refused. The patterns'
  meaning is the provider's, DuckDB's RE2. A limit the provider hits, an
  invalid pattern and text that is not UTF-8 are errors. The other regex
  forms stay refused with a table too: `~`, `!~`, `regexp_extract_all`,
  `regexp_split_to_array`, `regexp_split_to_table`, `string_split_regex`,
  `regexp_escape`, `regexp`, `regexp_like`, `regexp_match`,
  `regexp_extract` with a list of names, and `SIMILAR TO ... ESCAPE`.
- **UTF-8**, with a UTF-8 table: `length` counts the code points of valid
  text through it. The answer is the engine's own either way, so this
  table changes no result.

## Listed differences

None. Where the engine cannot give DuckDB's answer, it refuses the
statement (below). The text of `EXPLAIN`'s plan is the engine's own
(design §3).

## Refused

Each of these is an error, by design, until the engine holds what DuckDB
does:

- **Regular expressions** (design L24, L29, #40): `regexp_matches`,
  `regexp_replace`, `regexp_extract`, `regexp_full_match`,
  `string_split_regex`, `REGEXP`, `SIMILAR TO`, `~`, `!~` and every other
  regex function or operator, without a host's regex table; with one,
  those of [Functions a host provides](#functions-a-host-provides) are
  computed and the rest stay refused.
- **Mixed types that DuckDB casts at run time**: a `VARCHAR` column
  `IN` or `BETWEEN` numbers (DuckDB casts the column and fails on text that
  is no number), a `VALUES` column that mixes string literals with values
  of another type (DuckDB gives the literals that type, the engine's
  column was `VARCHAR`; #59), and text with a fraction or an exponent
  cast to an integer (`'1.5'::INTEGER` is 2 in DuckDB).
- **`DECIMAL` and `NUMERIC`** (#26): until a fixed-point type exists. A
  decimal literal is a `DOUBLE`.
- **The timestamp variants**: `TIMESTAMP_S`, `TIMESTAMP_MS`,
  `TIMESTAMP_NS`, `TIMESTAMPTZ`, `TIMETZ`, `TIMESTAMP(p)` other than 4 to
  6, `HUGEINT`, `INTERVAL`, `UUID`, the unsigned integers and the nested
  types. With them: `now()`, `current_timestamp`, `current_date` (DuckDB's
  are in the session's time zone, which the engine has not),
  `to_timestamp`, `age`, `date_add`, `date_sub`, `TIMESTAMP ± INTEGER`,
  `TIMESTAMP - TIMESTAMP` (an `INTERVAL` in DuckDB) and `DATE + BIGINT`.
- **Unicode case** (design L22, #39): `upper` and `lower` of text past
  ASCII, `ILIKE` with a pattern past ASCII, and `length` or `reverse`
  where a grapheme cluster may span several code points.
- **Files**: no statement reads or writes a file (design §2): the file
  table functions (`read_csv`, `read_parquet`, `read_json`, ...), a path
  used as a table, `DESCRIBE` of either, `COPY`, `ATTACH`, `INSTALL`,
  `LOAD` and `CREATE LIVE VIEW`.
- **Random numbers and UUIDs** (design L25): `random`, `setseed`, `uuid`,
  `gen_random_uuid`, until a host supplies them.
- **Transactions**: `BEGIN`, `COMMIT` and `ROLLBACK`. Every statement
  commits as it runs.
- **Functions DuckDB does not have** (design L4): MySQL's and others'
  forms the engine once accepted, among them `nvl`, `iif`, `getdate`,
  `curdate`, `date_format`, `dateadd`, `timestampadd`, `timestampdiff`,
  `subdate`, `from_unixtime`, `unix_timestamp` and the two-argument
  `datediff(d1, d2)`.
- **`version()` and `current_database()`**: their values are DuckDB's
  own.
