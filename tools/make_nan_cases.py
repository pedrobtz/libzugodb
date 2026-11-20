#!/usr/bin/env python3
"""DuckDB's answers to queries over NaN, the infinities, -0.0 and NULL.

The oracle of libzugodb #42 (design L30): each query below runs against
DuckDB's Python package at the tag in test/conformance/duckdb-tag, and its
result (or that it raised an error) is written to
test/fixtures/nan/duckdb_cases.tsv, which test/unit/execution/test_nan.cpp
replays against the engine at thread caps 1 and 2, and again after the
tables are saved to a database file and loaded.

    python3 -m venv venv && venv/bin/pip install duckdb==2.0.0
    venv/bin/python -I tools/make_nan_cases.py > test/fixtures/nan/duckdb_cases.tsv

Each line is `id <TAB> mode <TAB> sql <TAB> expected`. A mode of `setup`
is a statement the engine must run without error. Otherwise the mode is
`o` (rows in order) or `u` (rows as a multiset), followed by `L` where
-0.0 matches 0.0 (an ORDER BY or a group whose representative among equal
values DuckDB does not fix). A NaN matches any NaN, whatever its sign: the
sign of a computed NaN is the hardware's (it differs between x86-64 and
arm64), and only the VARCHAR casts below show it, of NaNs whose sign is
fixed. `expected` is `error`, or the rows separated by `|`, each value by
`,`: N (NULL), b0/b1, i<integer>, f<float.hex()> or fnan, s<text with
%, |, , and control bytes as %XX>.

The cases the engine refuses where DuckDB answers are listed, with their
reason, in test/fixtures/nan/refused.tsv; the doctest fails on any other
difference, and on a listed case that stops being refused.
"""

import decimal
import math
import sys

import duckdb

SPECIAL = {
    "nan": "'nan'::{t}",
    "-nan": "-('nan'::{t})",
    "inf": "'inf'::{t}",
    "-inf": "'-inf'::{t}",
    "zero": "'0.0'::{t}",
    "-zero": "'-0.0'::{t}",
    "1.5": "'1.5'::{t}",
    "-2": "'-2'::{t}",
    "null": "NULL::{t}",
}
TYPES = ["DOUBLE", "FLOAT"]
OPS = ["=", "<>", "<", ">", "<=", ">="]

# t: one row per special value, twice for NaN and 1.5; u: the join side.
T_ROWS = [
    (1, "nan", 1), (2, "-nan", 1), (3, "inf", 2), (4, "-inf", 2), (5, "zero", 3),
    (6, "-zero", 3), (7, "1.5", 1), (8, "-2", 2), (9, "null", 3), (10, "nan", 2),
    (11, "1.5", 3), (12, "-zero", 1),
]
U_ROWS = [(1, "-nan"), (2, "zero"), (3, "-zero"), (4, "inf"), (5, "1.5"), (6, "null"), (7, "nan")]


def lit(name, t):
    return SPECIAL[name].format(t=t)


cases = []


def add(sql, mode="u"):
    cases.append((sql, mode))


def setup(sql):
    cases.append((sql, "setup"))


# --- tables
setup("CREATE TABLE t (id INTEGER, d DOUBLE, f FLOAT, g INTEGER)")
setup("INSERT INTO t VALUES " + ", ".join(
    f"({i}, {lit(v, 'DOUBLE')}, {lit(v, 'FLOAT')}, {g})" for i, v, g in T_ROWS))
setup("CREATE TABLE u (id INTEGER, d DOUBLE, f FLOAT)")
setup("INSERT INTO u VALUES " + ", ".join(
    f"({i}, {lit(v, 'DOUBLE')}, {lit(v, 'FLOAT')})" for i, v in U_ROWS))
# A table past the fast paths' thresholds: the window's parallel sort
# (200,000 rows, two partitions), the join's hash table past its linear
# cache (256 build rows) and the ORDER BY heap.
setup("CREATE TABLE digits (x INTEGER)")
setup("INSERT INTO digits VALUES (0), (1), (2), (3), (4), (5), (6), (7), (8), (9)")
setup("CREATE TABLE two (x INTEGER)")
setup("INSERT INTO two VALUES (0), (1)")
setup("CREATE TABLE big AS SELECT i AS id, CASE i % 9 WHEN 0 THEN 'nan'::DOUBLE "
      "WHEN 1 THEN -('nan'::DOUBLE) WHEN 2 THEN 'inf'::DOUBLE WHEN 3 THEN '-inf'::DOUBLE "
      "WHEN 4 THEN '0.0'::DOUBLE WHEN 5 THEN '-0.0'::DOUBLE WHEN 6 THEN (i % 50)::DOUBLE "
      "WHEN 7 THEN NULL ELSE '1.5'::DOUBLE END AS d, i % 3 AS g "
      "FROM (SELECT a.x + 10 * b.x + 100 * c.x + 1000 * e.x + 10000 * f.x + 100000 * h.x AS i "
      "FROM digits a, digits b, digits c, digits e, digits f, two h) s")

# --- comparisons of constants, both types
names = list(SPECIAL)
for t in TYPES:
    for a in names:
        for b in names:
            exprs = [f"{lit(a, t)} {op} {lit(b, t)}" for op in OPS]
            exprs.append(f"{lit(a, t)} IS DISTINCT FROM {lit(b, t)}")
            exprs.append(f"{lit(a, t)} IS NOT DISTINCT FROM {lit(b, t)}")
            add("SELECT " + ", ".join(exprs), "o")
# across types and with integers
for a in names:
    for b in ["nan", "inf", "zero", "1.5"]:
        add(f"SELECT {lit(a, 'FLOAT')} = {lit(b, 'DOUBLE')}, {lit(a, 'FLOAT')} < {lit(b, 'DOUBLE')}", "o")
    add(f"SELECT {lit(a, 'DOUBLE')} > 1, {lit(a, 'DOUBLE')} < 9223372036854775807, "
        f"2 >= {lit(a, 'DOUBLE')}", "o")

# --- filters: column against a constant, and column against column
for col in ["d", "f"]:
    t = "DOUBLE" if col == "d" else "FLOAT"
    for c in names:
        for op in OPS:
            add(f"SELECT id FROM t WHERE {col} {op} {lit(c, t)}")
    add(f"SELECT id FROM t WHERE {col} BETWEEN 1 AND 'inf'::{t}")
    add(f"SELECT id FROM t WHERE {col} BETWEEN '-inf'::{t} AND 'nan'::{t}")
    add(f"SELECT id FROM t WHERE {col} NOT BETWEEN 0 AND 2")
    add(f"SELECT id FROM t WHERE {col} IN ('nan'::{t}, 1.5::{t})")
    add(f"SELECT id FROM t WHERE {col} IN ('-0.0'::{t})")
    add(f"SELECT id FROM t WHERE {col} NOT IN ('nan'::{t}, 'inf'::{t})")
    add(f"SELECT id FROM t WHERE {col} IN (1, 2, 3)")
    add(f"SELECT id FROM t WHERE {col} = {col}")
    add(f"SELECT id FROM t WHERE {col} <> {col}")
for op in OPS:
    add(f"SELECT a.id, b.id FROM t a, u b WHERE a.d {op} b.d")
    add(f"SELECT a.id, b.id FROM t a, u b WHERE a.f {op} b.d")
add("SELECT id FROM t WHERE isnan(d)")
add("SELECT id FROM t WHERE NOT isnan(f)")
add("SELECT id, CASE WHEN d = 'nan'::DOUBLE THEN 'n' WHEN d > 0 THEN 'p' ELSE 'o' END FROM t")
add("SELECT id, CASE d WHEN 'nan'::DOUBLE THEN 1 WHEN 0 THEN 2 ELSE 3 END FROM t")

# --- ORDER BY, both directions, NULLS FIRST and LAST, with and without LIMIT
for col in ["d", "f"]:
    for direction in ["", " DESC"]:
        for nulls in ["", " NULLS FIRST", " NULLS LAST"]:
            add(f"SELECT id, {col} FROM t ORDER BY {col}{direction}{nulls}, id", "o")
            for k in [1, 3, 5, 11]:
                add(f"SELECT {col} FROM t ORDER BY {col}{direction}{nulls} LIMIT {k}", "oL")
            add(f"SELECT id FROM t ORDER BY {col}{direction}{nulls}, id LIMIT 4", "o")
            add(f"SELECT id FROM t ORDER BY {col}{direction}{nulls}, id LIMIT 3 OFFSET 2", "o")
    add(f"SELECT id FROM t ORDER BY abs({col}), id", "o")
    add(f"SELECT id FROM t ORDER BY -({col}) DESC, id", "o")
    add(f"SELECT g, {col} FROM t ORDER BY g, {col} DESC, id", "o")
add("SELECT id FROM big ORDER BY d, id LIMIT 7", "o")
add("SELECT id FROM big ORDER BY d DESC, id LIMIT 7", "o")
add("SELECT d FROM big ORDER BY d DESC LIMIT 3", "oL")
add("SELECT d FROM big ORDER BY d LIMIT 3", "oL")

# --- GROUP BY, DISTINCT, COUNT(DISTINCT)
for col in ["d", "f"]:
    add(f"SELECT {col}, count(*), min(id), max(id) FROM t GROUP BY {col}")
    add(f"SELECT {col}, g, count(*) FROM t GROUP BY {col}, g")
    add(f"SELECT count(*) FROM t GROUP BY {col}")
    add(f"SELECT {col}, sum(id) FROM t GROUP BY {col}")
    add(f"SELECT DISTINCT {col} FROM t", "uL")
    add(f"SELECT DISTINCT {col}, g FROM t", "uL")
    add(f"SELECT count(DISTINCT {col}) FROM t")
    add(f"SELECT g, count(DISTINCT {col}) FROM t GROUP BY g")
    add(f"SELECT {col} IS NULL, count(*) FROM t GROUP BY {col} IS NULL")
add("SELECT d, count(*) FROM big GROUP BY d", "uL")
add("SELECT g, count(DISTINCT d) FROM big GROUP BY g")
add("SELECT count(*) FROM (SELECT DISTINCT d FROM big)")

# --- aggregates
AGGS = ["count({c})", "sum({c})", "avg({c})", "min({c})", "max({c})", "median({c})",
        "stddev_samp({c})", "var_pop({c})"]
for col in ["d", "f"]:
    add("SELECT " + ", ".join(a.format(c=col) for a in AGGS) + " FROM t", "o")
    add("SELECT g, " + ", ".join(a.format(c=col) for a in AGGS) + " FROM t GROUP BY g")
    add(f"SELECT min({col}), max({col}) FROM t WHERE NOT isnan({col})", "o")
    add(f"SELECT min({col}), max({col}), sum({col}) FROM t WHERE id IN (3, 4)", "o")
add("SELECT g, min(d), max(d), sum(d) FROM big GROUP BY g")
# over small sets of values, every type of each row given
subsets = [
    ["nan", "1.5"], ["1.5", "nan"], ["nan", "-nan"], ["-nan", "nan"], ["inf", "-inf"],
    ["zero", "-zero"], ["-zero", "zero"], ["zero", "-zero", "zero"], ["-zero", "1.5", "zero"],
    ["inf", "nan", "-inf"], ["null", "nan"], ["null", "null"], ["-2", "inf"], ["nan", "nan", "1.5"],
    ["-inf", "-2"], ["nan", "zero", "-zero"],
]
for t in TYPES:
    for s in subsets:
        rows = ", ".join(f"({i}, {lit(v, t)})" for i, v in enumerate(s))
        src = f"(VALUES {rows}) v(i, x)"
        add("SELECT " + ", ".join(a.format(c="x") for a in AGGS) + f" FROM {src}", "o")
        # The sign of the NaN or zero MIN and MAX keep: the last of equal
        # values. Every NaN here has a fixed sign (a literal or its negation).
        add(f"SELECT CAST(m AS VARCHAR), CAST(n AS VARCHAR) FROM (SELECT min(x) AS m, max(x) AS n "
            f"FROM {src})", "o")
        add(f"SELECT x, count(*) FROM {src} GROUP BY x", "u")

# --- joins: the hash join, outer joins, the join with an aggregate above it
for a_col, b_col in [("d", "d"), ("f", "f"), ("d", "f"), ("f", "d")]:
    add(f"SELECT a.id, b.id FROM t a JOIN u b ON a.{a_col} = b.{b_col}")
    add(f"SELECT a.id, b.id FROM t a LEFT JOIN u b ON a.{a_col} = b.{b_col}")
    add(f"SELECT a.id, b.id FROM u b RIGHT JOIN t a ON a.{a_col} = b.{b_col}")
    add(f"SELECT a.id, b.id FROM t a FULL JOIN u b ON a.{a_col} = b.{b_col}")
    add(f"SELECT count(*), sum(a.id) FROM t a JOIN u b ON a.{a_col} = b.{b_col}")
    add(f"SELECT b.id, count(*) FROM t a JOIN u b ON a.{a_col} = b.{b_col} GROUP BY b.id")
add("SELECT a.id, b.id FROM t a JOIN u b ON a.d = b.d AND a.f = b.f")
add("SELECT a.id, b.id FROM t a JOIN u b ON a.d = b.d AND a.id < b.id")
add("SELECT a.id, b.id FROM t a JOIN u b ON a.d IS NOT DISTINCT FROM b.d")
add("SELECT sum(a.d), min(a.d), max(a.f), avg(a.f) FROM t a JOIN u b ON a.id = b.id", "o")
add("SELECT b.d, count(*), max(a.d) FROM t a JOIN u b ON a.id = b.id GROUP BY b.d")
# a build side past the linear cache's 256 rows
add("SELECT count(*), sum(b.id) FROM t a JOIN (SELECT id, d FROM big WHERE id < 2000) b ON a.d = b.d")
add("SELECT a.id, count(*) FROM t a JOIN (SELECT id, d FROM big WHERE id < 2000) b ON a.d = b.d "
    "GROUP BY a.id")
add("SELECT count(*) FROM (SELECT id, d FROM big WHERE id < 2000) a JOIN u b ON a.d = b.d")
add("SELECT id FROM t WHERE d IN (SELECT d FROM u)")
add("SELECT id FROM t WHERE d NOT IN (SELECT d FROM u WHERE d IS NOT NULL)")
add("SELECT id FROM t WHERE EXISTS (SELECT 1 FROM u WHERE u.d = t.d)")

# --- set operations
for op in ["UNION", "INTERSECT", "EXCEPT", "UNION ALL"]:
    for col in ["d", "f"]:
        add(f"SELECT {col} FROM t {op} SELECT {col} FROM u", "uL")
    add(f"SELECT id % 2, d FROM t {op} SELECT id % 2, d FROM u", "uL")
add("SELECT count(*) FROM (SELECT d FROM t UNION SELECT d FROM u)")

# --- windows
for col in ["d", "f"]:
    add(f"SELECT id, rank() OVER (ORDER BY {col}) FROM t")
    add(f"SELECT id, dense_rank() OVER (ORDER BY {col} DESC) FROM t")
    add(f"SELECT id, row_number() OVER (ORDER BY {col}, id) FROM t")
    add(f"SELECT id, row_number() OVER (PARTITION BY {col} ORDER BY id) FROM t")
    add(f"SELECT id, count(*) OVER (PARTITION BY {col}) FROM t")
    add(f"SELECT id, sum(id) OVER (PARTITION BY {col}) FROM t")
    add(f"SELECT id, min({col}) OVER (PARTITION BY g), max({col}) OVER (PARTITION BY g) FROM t")
    add(f"SELECT id, sum({col}) OVER (PARTITION BY g ORDER BY id) FROM t")
    add(f"SELECT id, max({col}) OVER (PARTITION BY g ORDER BY id) FROM t")
    add(f"SELECT id, count(*) OVER (ORDER BY {col}) FROM t")
    add(f"SELECT id, lag({col}) OVER (ORDER BY id), lead({col}) OVER (ORDER BY id) FROM t")
    add(f"SELECT id, first_value({col}) OVER (PARTITION BY g ORDER BY id) FROM t")
    add(f"SELECT id FROM t QUALIFY row_number() OVER (PARTITION BY g ORDER BY {col}, id) = 1")
    add(f"SELECT id FROM t QUALIFY rank() OVER (ORDER BY {col}) <= 3")
    add(f"SELECT id, ntile(3) OVER (ORDER BY {col}, id) FROM t")
add("SELECT g, sum(r), max(r) FROM (SELECT g, rank() OVER (PARTITION BY g ORDER BY d) AS r FROM big) "
    "GROUP BY g")
add("SELECT g, sum(r) FROM (SELECT g, row_number() OVER (PARTITION BY g ORDER BY d, id) AS r FROM big "
    "QUALIFY r <= 10) GROUP BY g")
add("SELECT count(*) FROM (SELECT d, dense_rank() OVER (ORDER BY d) AS r FROM big) WHERE r = 1")

# --- casts
for t in TYPES:
    for c in names:
        for target in ["INTEGER", "BIGINT", "SMALLINT", "BOOLEAN"]:
            add(f"SELECT CAST({lit(c, t)} AS {target})", "o")
            add(f"SELECT TRY_CAST({lit(c, t)} AS {target})", "o")
        add(f"SELECT CAST({lit(c, t)} AS VARCHAR)", "o")
        add(f"SELECT CAST({lit(c, t)} AS DOUBLE), CAST({lit(c, t)} AS FLOAT)", "o")
for col in ["d", "f"]:
    add(f"SELECT id, CAST({col} AS VARCHAR) FROM t")
    add(f"SELECT id, TRY_CAST({col} AS INTEGER) FROM t")
    add(f"SELECT id, CAST({col} AS BOOLEAN) FROM t")
    add(f"SELECT id, {col} || '' FROM t")
    add(f"SELECT id, CAST({col} AS INTEGER) FROM t WHERE {col} = 1.5")
    add(f"SELECT CAST({col} AS INTEGER) FROM t")
TEXTS = ["nan", "NaN", "NAN", "-nan", "+nan", " nan ", "nan(123)", "nan()", "nan(", "nanx", "inf",
         "-inf", "+inf", "infinity", "-Infinity", "INF", "infinit", "1.5", " 2 ", "-0.0", "-0",
         "0x10", "1e", "", "1e400", "1e-400"]
for t in TYPES:
    for s in TEXTS:
        q = "'" + s.replace("'", "''") + "'"
        add(f"SELECT CAST({q} AS {t})", "o")
        add(f"SELECT TRY_CAST({q} AS {t})", "o")
    add(f"SELECT CAST(CAST('nan' AS {t}) AS VARCHAR), CAST(-CAST('nan' AS {t}) AS VARCHAR)", "o")
for s in ["nan", "-nan", "inf", "1.5"]:
    add(f"SELECT CAST('{s}' AS INTEGER)", "o")
    add(f"SELECT TRY_CAST('{s}' AS INTEGER)", "o")
add("SELECT CAST(1e39::DOUBLE AS FLOAT)", "o")
add("SELECT CAST('1e39' AS FLOAT)", "o")
add("SELECT CAST(3.4028235e38::DOUBLE AS FLOAT)", "o")

# --- arithmetic and functions over the special values
ARITH = ["+", "-", "*", "/", "%"]
for t in TYPES:
    for a in names:
        for b in names:
            add("SELECT " + ", ".join(f"{lit(a, t)} {op} {lit(b, t)}" for op in ARITH), "o")
FUNCS1 = ["abs({x})", "sign({x})", "ceil({x})", "floor({x})", "round({x})", "round({x}, 2)",
          "round({x}, -1)", "trunc({x})", "sqrt({x})", "cbrt({x})", "ln({x})", "log10({x})",
          "log2({x})", "log({x})", "exp({x})", "sin({x})", "cos({x})", "tan({x})", "asin({x})",
          "acos({x})", "atan({x})", "isnan({x})", "isinf({x})", "isfinite({x})", "-({x})",
          "power({x}, 2)", "power(2, {x})", "power({x}, 0)", "power(1, {x})", "atan2({x}, 1)",
          "atan2(1, {x})", "greatest({x}, 1)", "least({x}, 1)", "greatest(1, {x}, 'nan'::DOUBLE)",
          "nullif({x}, 'nan'::DOUBLE)", "coalesce({x}, 7)", "degrees({x})", "radians({x})",
          "sinh({x})", "cosh({x})", "tanh({x})", "asinh({x})", "acosh({x})", "atanh({x})"]
for t in TYPES:
    for a in names:
        x = lit(a, t)
        add("SELECT " + ", ".join(f.format(x=x) for f in FUNCS1), "o")
for c in ["nan", "-nan", "inf", "-inf", "zero", "-zero", "1.5"]:
    x = lit(c, "DOUBLE")
    add(f"SELECT CAST(abs({x}) AS VARCHAR), CAST(-({x}) AS VARCHAR), CAST(greatest({x}, 1) AS VARCHAR)", "o")
for col in ["d", "f"]:
    add(f"SELECT id, {col} + 1, {col} * 0, {col} / 0, {col} % 2, -({col}), abs({col}) FROM t")
    add(f"SELECT id, sqrt({col}), ln({col}), exp({col}), sign({col}), round({col}, 1) FROM t")
    add(f"SELECT id, greatest({col}, 0), least({col}, 0), nullif({col}, 'nan'::DOUBLE) FROM t")
add("SELECT 0.0 / 0.0, 1.0 / 0.0, -1.0 / 0.0", "o")
add("SELECT 10.0 % 0.0", "o")
add("SELECT 10 % 0.0", "o")
add("SELECT mod(10.0, 0.0)", "o")
add("SELECT 'inf'::DOUBLE % 0.0", "o")
add("SELECT 1.0 / -0.0, -0.0 * 1", "o")


def enc_value(v):
    if v is None:
        return "N"
    if isinstance(v, bool):
        return "b1" if v else "b0"
    if isinstance(v, int):
        return "i" + str(v)
    if isinstance(v, decimal.Decimal):
        # A decimal literal's arithmetic, a DOUBLE in the engine (design §3).
        v = float(v)
    if isinstance(v, float):
        return "fnan" if math.isnan(v) else "f" + v.hex()
    if isinstance(v, str):
        out = []
        for ch in v:
            if ch in "%|,\t\n\r" or ord(ch) < 32:
                out.append("%%%02X" % ord(ch))
            else:
                out.append(ch)
        return "s" + "".join(out)
    raise TypeError(f"no encoding for {type(v).__name__}: {v!r}")


def main():
    con = duckdb.connect()
    con.execute("SET threads = 1")
    out = sys.stdout
    out.write(f"# DuckDB {duckdb.__version__} (Python package), tools/make_nan_cases.py; "
              "see its docstring for the format.\n")
    for n, (sql, mode) in enumerate(cases, 1):
        if mode == "setup":
            con.execute(sql)
            out.write(f"{n}\tsetup\t{sql}\t\n")
            continue
        try:
            rows = con.execute(sql).fetchall()
            expected = "|".join(",".join(enc_value(v) for v in row) for row in rows)
        except duckdb.Error:
            expected = "error"
        out.write(f"{n}\t{mode}\t{sql}\t{expected}\n")


if __name__ == "__main__":
    main()
