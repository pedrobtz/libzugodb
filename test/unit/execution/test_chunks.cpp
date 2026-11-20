// Chunk capacity at the vector boundary (libzugodb Stage 10, #14).
//
// A DataChunk holds VECTOR_SIZE (2048) rows unless its creator sizes it.
// Each query here has an intermediate result of 2047, 2048, 2049, 4095,
// 4096 or 4097 rows, with NULLs on each side of each boundary, and is run
// twice: over the table t, and over the same rows as a UNION ALL of two
// halves. The row count and an order-independent checksum of the values
// must be equal, and equal to the count the query must give where it is
// known. Under ASan (the CI job) an operator that writes past a chunk's
// capacity is a report. design.md L27 lists the sites and the case that
// reaches each.

#include "doctest.h"
#include "zugodb/common/exception.hpp"
#include "zugodb/main/connection.hpp"
#include "zugodb/main/database.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

using namespace zugodb;

namespace {

const std::vector<int64_t> &Sizes() {
    static const std::vector<int64_t> sizes = {2047, 2048, 2049, 4095, 4096, 4097};
    return sizes;
}

// Rows whose columns other than r are NULL: each side of each boundary,
// the first and the last.
std::set<int64_t> NullRows(int64_t n) {
    std::set<int64_t> out;
    for (int64_t v : {int64_t(0), int64_t(2047), int64_t(2048), int64_t(4095), int64_t(4096), n - 1}) {
        if (v >= 0 && v < n) out.insert(v);
    }
    return out;
}

// One row of t: r = v (never NULL); i = v; k = v % 7; s = 's' || v, or a
// string longer than the inline 12 bytes for every fifth row; d = v / 4.
std::vector<Value> Row(int64_t v, bool null) {
    if (null) return {Value::BIGINT(v), Value(), Value(), Value(), Value()};
    std::string s = (v % 5 == 0) ? "a-longer-string-" + std::to_string(v) : "s" + std::to_string(v);
    return {Value::BIGINT(v), Value::BIGINT(v), Value::INTEGER((int32_t)(v % 7)), Value::VARCHAR(s),
            Value::DOUBLE((double)v / 4.0)};
}

const char *SCHEMA = "(r BIGINT, i BIGINT, k INTEGER, s VARCHAR, d DOUBLE)";

// Rows lo .. hi - 1 into `name`, through the C++ API.
void Fill(Connection &conn, const std::string &name, int64_t lo, int64_t hi, const std::set<int64_t> &nulls) {
    conn.Query("CREATE TABLE " + name + " " + SCHEMA);
    for (int64_t base = lo; base < hi; base += (int64_t)VECTOR_SIZE) {
        int64_t m = std::min<int64_t>((int64_t)VECTOR_SIZE, hi - base);
        DataChunk chunk;
        chunk.Initialize({LogicalType::BIGINT(), LogicalType::BIGINT(), LogicalType::INTEGER(),
                          LogicalType::VARCHAR(), LogicalType::DOUBLE()});
        for (int64_t j = 0; j < m; j++) {
            auto row = Row(base + j, nulls.count(base + j) > 0);
            for (idx_t c = 0; c < row.size(); c++) chunk.SetValue(c, (idx_t)j, row[c]);
        }
        chunk.SetCardinality((idx_t)m);
        conn.Append(name, chunk);
    }
}

// t holds 0 .. n - 1; ta and tb its two halves; small(k, w) the keys
// 0 .. 6 three times; fifty(w) 0 .. 49.
void MakeTables(Connection &conn, int64_t n) {
    auto nulls = NullRows(n);
    Fill(conn, "t", 0, n, nulls);
    Fill(conn, "ta", 0, n / 2, nulls);
    Fill(conn, "tb", n / 2, n, nulls);
    conn.Query("CREATE TABLE small (k INTEGER, w INTEGER)");
    std::string values;
    for (int rep = 0; rep < 3; rep++) {
        for (int k = 0; k < 7; k++) {
            if (!values.empty()) values += ", ";
            values += "(" + std::to_string(k) + ", " + std::to_string(rep) + ")";
        }
    }
    conn.Query("INSERT INTO small VALUES " + values);
    conn.Query("CREATE TABLE fifty (w INTEGER)");
    values.clear();
    for (int w = 0; w < 50; w++) values += (w ? ", (" : "(") + std::to_string(w) + ")";
    conn.Query("INSERT INTO fifty VALUES " + values);
}

const std::string HALVES = "(SELECT * FROM ta UNION ALL SELECT * FROM tb)";

uint64_t Fnv(const std::string &s) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

struct Summary {
    idx_t rows = 0;
    uint64_t checksum = 0;
    idx_t nulls = 0;
};

// The row count, the sum of each row's hash (order-independent) and the
// number of NULL values.
Summary Summarise(QueryResult &r) {
    Summary out;
    out.rows = r.RowCount();
    for (idx_t i = 0; i < r.RowCount(); i++) {
        std::string line;
        for (idx_t j = 0; j < r.ColumnCount(); j++) {
            auto &v = r.GetValue(i, j);
            if (v.IsNull()) out.nulls++;
            line += v.IsNull() ? std::string("NULL") : v.ToString();
            line += '|';
        }
        out.checksum += Fnv(line);
    }
    return out;
}

std::string Replace(std::string sql, const std::string &with) {
    for (size_t p = sql.find("{t}"); p != std::string::npos; p = sql.find("{t}", p + with.size())) {
        sql.replace(p, 3, with);
    }
    return sql;
}

// What a query must give for t of n rows, nn of them the NULL rows; -1 if
// the test does not know.
using Expect = std::function<int64_t(int64_t n, int64_t nn)>;

struct Case {
    const char *name;
    const char *sql; // {t} is t, then the halves
    Expect rows;
};

int64_t Odd(int64_t n) { return n / 2; }  // r % 2 = 1 in 0 .. n - 1

const std::vector<Case> &Cases() {
    static const std::vector<Case> cases = {
        // Scan, filter and projection.
        {"scan", "SELECT r, i, k, s, d FROM {t}", [](int64_t n, int64_t) { return n; }},
        {"filter-all", "SELECT r, i, s FROM {t} WHERE r >= 0", [](int64_t n, int64_t) { return n; }},
        {"filter-odd", "SELECT r, s, d FROM {t} WHERE r % 2 = 1", [](int64_t n, int64_t) { return Odd(n); }},
        {"filter-null", "SELECT r FROM {t} WHERE i IS NULL", [](int64_t, int64_t nn) { return nn; }},
        {"filter-string", "SELECT r, s FROM {t} WHERE s LIKE '%1%' OR s IS NULL", nullptr},
        {"literal-emit", "SELECT z FROM (SELECT r, 1 AS z FROM {t}) q WHERE z = 1",
         [](int64_t n, int64_t) { return n; }},
        {"projection",
         "SELECT r + 1, upper(s), d * 2, CASE WHEN i IS NULL THEN 'n' ELSE s END, coalesce(k, -1), "
         "length(s), i IS NULL FROM {t}",
         [](int64_t n, int64_t) { return n; }},
        // Sorts, top-k and limits.
        {"order", "SELECT r, i, s FROM {t} ORDER BY r", [](int64_t n, int64_t) { return n; }},
        {"order-string", "SELECT s, r FROM {t} ORDER BY s, r", [](int64_t n, int64_t) { return n; }},
        {"order-nulls", "SELECT i, d FROM {t} ORDER BY i DESC, r", [](int64_t n, int64_t) { return n; }},
        {"topk-2049", "SELECT r, s FROM {t} ORDER BY r DESC LIMIT 2049",
         [](int64_t n, int64_t) { return std::min<int64_t>(n, 2049); }},
        {"topk-2048-string", "SELECT s FROM {t} ORDER BY s LIMIT 2048",
         [](int64_t n, int64_t) { return std::min<int64_t>(n, 2048); }},
        {"topk-offset", "SELECT r, i FROM {t} ORDER BY r LIMIT 2048 OFFSET 1",
         [](int64_t n, int64_t) { return std::min<int64_t>(n - 1, 2048); }},
        {"limit", "SELECT r, s FROM {t} LIMIT 2049", [](int64_t n, int64_t) { return std::min<int64_t>(n, 2049); }},
        {"limit-offset", "SELECT r FROM {t} LIMIT 4096 OFFSET 2047",
         [](int64_t n, int64_t) { return std::min<int64_t>(n - 2047, 4096); }},
        // DISTINCT and GROUP BY with more groups than one vector.
        {"distinct-r", "SELECT DISTINCT r FROM {t}", [](int64_t n, int64_t) { return n; }},
        {"distinct-i", "SELECT DISTINCT i FROM {t}", [](int64_t n, int64_t nn) { return n - nn + 1; }},
        {"distinct-s", "SELECT DISTINCT s FROM {t}", [](int64_t n, int64_t nn) { return n - nn + 1; }},
        {"distinct-two", "SELECT DISTINCT k, s FROM {t}", [](int64_t n, int64_t nn) { return n - nn + 1; }},
        {"group-r", "SELECT r, count(*), sum(d) FROM {t} GROUP BY r", [](int64_t n, int64_t) { return n; }},
        {"group-i", "SELECT i, count(*), max(s) FROM {t} GROUP BY i",
         [](int64_t n, int64_t nn) { return n - nn + 1; }},
        {"group-s", "SELECT s, count(*), min(r) FROM {t} GROUP BY s",
         [](int64_t n, int64_t nn) { return n - nn + 1; }},
        {"group-k", "SELECT k, count(*), sum(r), count(i), avg(d), min(s) FROM {t} GROUP BY k",
         [](int64_t, int64_t) { return 8; }},
        {"group-two", "SELECT k, s, count(*) FROM {t} GROUP BY k, s",
         [](int64_t n, int64_t nn) { return n - nn + 1; }},
        {"group-expr", "SELECT r % 3000 AS g, count(*), sum(i) FROM {t} GROUP BY g",
         [](int64_t n, int64_t) { return std::min<int64_t>(n, 3000); }},
        {"group-having", "SELECT r, count(*) FROM {t} GROUP BY r HAVING count(*) = 1",
         [](int64_t n, int64_t) { return n; }},
        {"group-count-distinct", "SELECT k, count(DISTINCT s), count(DISTINCT i) FROM {t} GROUP BY k",
         [](int64_t, int64_t) { return 8; }},
        {"group-order-limit", "SELECT r, count(*) AS c FROM {t} GROUP BY r ORDER BY c DESC, r LIMIT 2049",
         [](int64_t n, int64_t) { return std::min<int64_t>(n, 2049); }},
        {"aggregate",
         "SELECT count(*), count(i), sum(i), min(s), max(s), avg(d), count(DISTINCT i), count(DISTINCT s), "
         "min(r), max(r) FROM {t}",
         [](int64_t, int64_t) { return 1; }},
        {"aggregate-case-group",
         "SELECT k, count(CASE WHEN r % 2 = 0 THEN 1 END), sum(CASE WHEN r % 3 = 0 THEN d END), "
         "sum(r + 1), max(r * 2) FROM {t} GROUP BY k",
         [](int64_t, int64_t) { return 8; }},
        {"aggregate-case",
         "SELECT count(CASE WHEN r % 2 = 0 THEN 1 END), sum(CASE WHEN r % 3 = 0 THEN d END), avg(r * 2), "
         "count(DISTINCT r % 3000) FROM {t}",
         [](int64_t, int64_t) { return 1; }},
        {"aggregate-filter", "SELECT count(*), sum(d) FROM {t} WHERE k = 3", [](int64_t, int64_t) { return 1; }},
        // Window functions.
        {"window-row-number", "SELECT r, row_number() OVER (ORDER BY r) FROM {t}",
         [](int64_t n, int64_t) { return n; }},
        {"window-partition", "SELECT r, sum(d) OVER (PARTITION BY k), count(*) OVER (PARTITION BY k) FROM {t}",
         [](int64_t n, int64_t) { return n; }},
        {"window-lag-lead", "SELECT r, lag(i) OVER (ORDER BY r), lead(s) OVER (ORDER BY r) FROM {t}",
         [](int64_t n, int64_t) { return n; }},
        {"window-rank", "SELECT r, rank() OVER (PARTITION BY k ORDER BY r) FROM {t}",
         [](int64_t n, int64_t) { return n; }},
        {"window-running", "SELECT r, sum(r) OVER (ORDER BY r) FROM {t}", [](int64_t n, int64_t) { return n; }},
        {"window-expression", "SELECT r + 1, upper(s), row_number() OVER (ORDER BY r) FROM {t}",
         [](int64_t n, int64_t) { return n; }},
        {"window-partition-expression", "SELECT r, count(*) OVER (PARTITION BY r % 2) FROM {t}",
         [](int64_t n, int64_t) { return n; }},
        {"window-two-windows", "SELECT r, row_number() OVER (ORDER BY r), sum(d) OVER (PARTITION BY k) FROM {t}",
         [](int64_t n, int64_t) { return n; }},
        {"window-two-windows-expression",
         "SELECT r * 2, rank() OVER (PARTITION BY k ORDER BY r), count(*) OVER (PARTITION BY r % 3) FROM {t}",
         [](int64_t n, int64_t) { return n; }},
        {"window-partition-s", "SELECT r, count(*) OVER (PARTITION BY s), lag(r) OVER (PARTITION BY i ORDER BY r) FROM {t}",
         [](int64_t n, int64_t) { return n; }},
        {"qualify", "SELECT r, k FROM {t} QUALIFY row_number() OVER (PARTITION BY k ORDER BY r) <= 400",
         nullptr},
        // Joins.
        {"join-r", "SELECT a.r, b.s FROM {t} a JOIN {t} b ON a.r = b.r", [](int64_t n, int64_t) { return n; }},
        {"join-i", "SELECT a.r, b.d FROM {t} a JOIN {t} b ON a.i = b.i",
         [](int64_t n, int64_t nn) { return n - nn; }},
        {"join-s", "SELECT a.r, b.r FROM {t} a JOIN {t} b ON a.s = b.s",
         [](int64_t n, int64_t nn) { return n - nn; }},
        {"join-two-keys", "SELECT a.r FROM {t} a JOIN {t} b ON a.i = b.i AND a.k = b.k",
         [](int64_t n, int64_t nn) { return n - nn; }},
        {"join-left", "SELECT a.r, b.r FROM {t} a LEFT JOIN {t} b ON a.i = b.i",
         [](int64_t n, int64_t) { return n; }},
        {"join-right", "SELECT a.r, b.r FROM {t} a RIGHT JOIN {t} b ON a.i = b.i",
         [](int64_t n, int64_t) { return n; }},
        {"join-full", "SELECT a.r, b.r FROM {t} a FULL OUTER JOIN {t} b ON a.i = b.i",
         [](int64_t n, int64_t nn) { return n + nn; }},
        {"join-fanout", "SELECT a.r, b.w FROM {t} a JOIN small b ON a.k = b.k",
         [](int64_t n, int64_t nn) { return 3 * (n - nn); }},
        {"join-fanout-left", "SELECT a.r, b.w FROM {t} a LEFT JOIN small b ON a.k = b.k",
         [](int64_t n, int64_t nn) { return 3 * (n - nn) + nn; }},
        {"join-build-small", "SELECT b.w, a.r FROM small b JOIN {t} a ON b.k = a.k",
         [](int64_t n, int64_t nn) { return 3 * (n - nn); }},
        {"join-cross", "SELECT a.r, b.w FROM {t} a CROSS JOIN small b", [](int64_t n, int64_t) { return 21 * n; }},
        {"join-group", "SELECT a.k, count(*), sum(b.d) FROM {t} a JOIN {t} b ON a.r = b.r GROUP BY a.k",
         [](int64_t, int64_t) { return 8; }},
        {"join-group-many", "SELECT a.r, count(*) FROM {t} a JOIN small b ON a.k = b.k GROUP BY a.r",
         [](int64_t n, int64_t nn) { return n - nn; }},
        {"join-left-large",
         "SELECT x.w, count(*), sum(y.d) FROM (SELECT a.r AS r, b.w AS w FROM {t} a CROSS JOIN fifty b) x "
         "JOIN {t} y ON x.r = y.r GROUP BY x.w",
         [](int64_t, int64_t) { return 50; }},
        {"join-left-large-rows",
         "SELECT x.r, y.s FROM (SELECT a.r AS r FROM {t} a CROSS JOIN fifty b) x JOIN {t} y ON x.r = y.r "
         "WHERE x.r % 50 = 0",
         nullptr},
        {"join-inequality", "SELECT a.r, b.w FROM {t} a JOIN small b ON a.k < b.k", nullptr},
        {"semi-in", "SELECT r FROM {t} WHERE i IN (SELECT i FROM {t})", [](int64_t n, int64_t nn) { return n - nn; }},
        {"anti-not-in", "SELECT r FROM {t} WHERE r NOT IN (SELECT r FROM {t} WHERE r % 2 = 0)",
         [](int64_t n, int64_t) { return Odd(n); }},
        {"semi-exists", "SELECT r FROM {t} WHERE EXISTS (SELECT 1 FROM {t} WHERE r > 0)",
         [](int64_t n, int64_t) { return n; }},
        {"scalar-subquery", "SELECT r, (SELECT max(r) FROM {t}) FROM {t}", [](int64_t n, int64_t) { return n; }},
        // Set operations, derived tables and CTEs.
        {"union-all", "SELECT r, s FROM {t} UNION ALL SELECT r, s FROM {t}", [](int64_t n, int64_t) { return 2 * n; }},
        {"union", "SELECT r, s FROM {t} UNION SELECT r, s FROM {t}", [](int64_t n, int64_t) { return n; }},
        {"intersect", "SELECT r, i FROM {t} INTERSECT SELECT r, i FROM {t}", [](int64_t n, int64_t) { return n; }},
        {"except", "SELECT r FROM {t} EXCEPT SELECT r FROM {t} WHERE r % 2 = 0",
         [](int64_t n, int64_t) { return Odd(n); }},
        {"derived", "SELECT q.r, q.s FROM (SELECT r, s FROM {t} WHERE r >= 0) q",
         [](int64_t n, int64_t) { return n; }},
        {"cte", "WITH c AS (SELECT r, s, d FROM {t}) SELECT * FROM c", [](int64_t n, int64_t) { return n; }},
        {"cte-join", "WITH c AS (SELECT r, i FROM {t}) SELECT x.r, y.i FROM c x JOIN c y ON x.r = y.r",
         [](int64_t n, int64_t) { return n; }},
    };
    return cases;
}

void CheckCase(Connection &conn, const Case &c, int64_t n, int64_t nn) {
    std::string name = c.name;
    CAPTURE(name);
    CAPTURE(n);
    auto whole_sql = Replace(c.sql, "t");
    auto halves_sql = Replace(c.sql, HALVES);
    CAPTURE(whole_sql);
    auto whole = conn.Query(whole_sql);
    auto halves = conn.Query(halves_sql);
    auto a = Summarise(whole);
    auto b = Summarise(halves);
    CHECK(a.rows == b.rows);
    CHECK(a.checksum == b.checksum);
    CHECK(a.nulls == b.nulls);
    if (c.rows) CHECK((int64_t)a.rows == c.rows(n, nn));
}

// The summary of a table, for statements that make one.
Summary Table(Connection &conn, const std::string &name) {
    auto r = conn.Query("SELECT r, i, k, s, d FROM " + name);
    return Summarise(r);
}

void CheckSame(const Summary &a, const Summary &b) {
    CHECK(a.rows == b.rows);
    CHECK(a.checksum == b.checksum);
    CHECK(a.nulls == b.nulls);
}

std::string Literal(const Value &v) {
    if (v.IsNull()) return "NULL";
    if (v.type().id() == LogicalTypeId::VARCHAR) return "'" + v.ToString() + "'";
    return v.ToString();
}

// t's rows as one VALUES list.
std::string ValuesList(int64_t n) {
    auto nulls = NullRows(n);
    std::string out;
    for (int64_t v = 0; v < n; v++) {
        auto row = Row(v, nulls.count(v) > 0);
        if (v) out += ", ";
        out += "(";
        for (idx_t c = 0; c < row.size(); c++) {
            if (c) out += ", ";
            out += Literal(row[c]);
        }
        out += ")";
    }
    return out;
}

} // namespace

TEST_CASE("Chunks: queries over 2047 to 4097 rows equal the same queries over two halves") {
    for (int64_t n : Sizes()) {
        Database db;
        Connection conn(db);
        MakeTables(conn, n);
        int64_t nn = (int64_t)NullRows(n).size();
        for (auto &c : Cases()) CheckCase(conn, c, n, nn);
    }
}

TEST_CASE("Chunks: statements that make a table of 2047 to 4097 rows keep every row and NULL") {
    for (int64_t n : Sizes()) {
        CAPTURE(n);
        Database db;
        Connection conn(db);
        MakeTables(conn, n);
        auto want = Table(conn, "t");
        CHECK((int64_t)want.rows == n);

        SUBCASE("INSERT ... SELECT") {
            conn.Query(std::string("CREATE TABLE u ") + SCHEMA);
            conn.Query("INSERT INTO u SELECT * FROM t");
            CheckSame(Table(conn, "u"), want);
            conn.Query(std::string("CREATE TABLE w ") + SCHEMA);
            conn.Query("INSERT INTO w SELECT * FROM " + HALVES + " h");
            CheckSame(Table(conn, "w"), want);
        }
        SUBCASE("INSERT ... SELECT, columns named in another order") {
            conn.Query(std::string("CREATE TABLE u ") + SCHEMA);
            conn.Query("INSERT INTO u (d, s, k, i, r) SELECT d, s, k, i, r FROM t");
            CheckSame(Table(conn, "u"), want);
        }
        SUBCASE("INSERT ... SELECT, a column left out") {
            conn.Query(std::string("CREATE TABLE u ") + SCHEMA);
            conn.Query("INSERT INTO u (r, i, k, s) SELECT r, i, k, s FROM t");
            auto got = conn.Query("SELECT count(*), count(d) FROM u");
            CHECK(got.GetValue(0, 0).GetValue<int64_t>() == n);
            CHECK(got.GetValue(0, 1).GetValue<int64_t>() == 0);
            auto a = conn.Query("SELECT r, i, k, s FROM u");
            auto b = conn.Query("SELECT r, i, k, s FROM t");
            auto sa = Summarise(a), sb = Summarise(b);
            CheckSame(sa, sb);
        }
        SUBCASE("INSERT ... SELECT, values cast to the column's type") {
            conn.Query("CREATE TABLE u (r BIGINT, i DOUBLE, k BIGINT, s VARCHAR, d DOUBLE)");
            conn.Query("INSERT INTO u SELECT * FROM t");
            auto a = conn.Query("SELECT r, CAST(i AS BIGINT), k, s, d FROM u");
            auto sa = Summarise(a);
            CheckSame(sa, want);
        }
        SUBCASE("CREATE TABLE AS") {
            conn.Query("CREATE TABLE u AS SELECT * FROM t");
            CheckSame(Table(conn, "u"), want);
            conn.Query("CREATE TABLE w AS SELECT * FROM " + HALVES + " h");
            CheckSame(Table(conn, "w"), want);
        }
        SUBCASE("INSERT ... VALUES") {
            conn.Query(std::string("CREATE TABLE u ") + SCHEMA);
            conn.Query("INSERT INTO u VALUES " + ValuesList(n));
            CheckSame(Table(conn, "u"), want);
        }
        SUBCASE("INSERT ... VALUES into a table with a primary key") {
            conn.Query("CREATE TABLE u (r BIGINT PRIMARY KEY, i BIGINT, k INTEGER, s VARCHAR, d DOUBLE)");
            conn.Query("INSERT INTO u VALUES " + ValuesList(n));
            CheckSame(Table(conn, "u"), want);
            CHECK_THROWS(conn.Query("INSERT INTO u VALUES (" + std::to_string(n - 1) + ", 1, 1, 'x', 1)"));
        }
        SUBCASE("SELECT from VALUES") {
            auto r = conn.Query("SELECT * FROM (VALUES " + ValuesList(n) + ") v(r, i, k, s, d)");
            CheckSame(Summarise(r), want);
            auto f = conn.Query("SELECT * FROM (VALUES " + ValuesList(n) + ") v(r, i, k, s, d) WHERE r >= 0");
            CheckSame(Summarise(f), want);
        }
        SUBCASE("UPDATE") {
            conn.Query("CREATE TABLE u AS SELECT * FROM t");
            conn.Query("UPDATE u SET s = s || 'x', d = d + 1 WHERE r % 2 = 0");
            auto a = conn.Query("SELECT r, i, k, s, d FROM u");
            auto b = conn.Query(
                "SELECT r, i, k, CASE WHEN r % 2 = 0 THEN s || 'x' ELSE s END, "
                "CASE WHEN r % 2 = 0 THEN d + 1 ELSE d END FROM t");
            CheckSame(Summarise(a), Summarise(b));
        }
        SUBCASE("DELETE") {
            conn.Query("CREATE TABLE u AS SELECT * FROM t");
            conn.Query("DELETE FROM u WHERE r % 3 = 0");
            auto a = conn.Query("SELECT r, i, k, s, d FROM u");
            auto b = conn.Query("SELECT r, i, k, s, d FROM t WHERE r % 3 <> 0");
            CheckSame(Summarise(a), Summarise(b));
        }
        SUBCASE("ALTER TABLE") {
            conn.Query("CREATE TABLE u AS SELECT * FROM t");
            conn.Query("ALTER TABLE u ADD COLUMN e INTEGER");
            auto a = conn.Query("SELECT count(*), count(e) FROM u");
            CHECK(a.GetValue(0, 0).GetValue<int64_t>() == n);
            CHECK(a.GetValue(0, 1).GetValue<int64_t>() == 0);
            CheckSame(Table(conn, "u"), want);
            conn.Query("ALTER TABLE u DROP COLUMN e");
            CheckSame(Table(conn, "u"), want);
            conn.Query("ALTER TABLE u RENAME COLUMN s TO s2");
            auto b = conn.Query("SELECT r, i, k, s2, d FROM u");
            CheckSame(Summarise(b), want);
        }
        SUBCASE("MERGE") {
            conn.Query("CREATE TABLE u AS SELECT r, i, k, 'old' AS s, d FROM t WHERE r % 2 = 0");
            conn.Query("MERGE INTO u USING t ON u.r = t.r WHEN MATCHED THEN UPDATE SET s = t.s "
                       "WHEN NOT MATCHED THEN INSERT VALUES (t.r, t.i, t.k, t.s, t.d)");
            CheckSame(Table(conn, "u"), want);
        }
        SUBCASE("A view") {
            conn.Query("CREATE VIEW v AS SELECT * FROM t WHERE r >= 0");
            CheckSame(Table(conn, "v"), want);
        }
        SUBCASE("The C++ scan") {
            Summary s;
            conn.Scan("t", [&](DataChunk &chunk) {
                CHECK(chunk.size() <= VECTOR_SIZE);
                s.rows += chunk.size();
            });
            CHECK((int64_t)s.rows == n);
        }
    }
}

TEST_CASE("Chunks: row generators of 2047 to 4097 rows") {
    for (int64_t n : Sizes()) {
        CAPTURE(n);
        Database db;
        Connection conn(db);
        auto expect = [&](const std::string &sql) {
            CAPTURE(sql);
            auto r = conn.Query(sql);
            REQUIRE(r.RowCount() == 1);
            CHECK(r.GetValue(0, 0).GetValue<int64_t>() == n);
            CHECK(r.GetValue(0, 1).GetValue<int64_t>() == n * (n - 1) / 2);
        };
        auto last = std::to_string(n - 1);
        expect("SELECT count(*), sum(x) FROM generate_series(0, " + last + ") g(x)");
        expect("SELECT count(*), sum(x) FROM generate_series(0, " + last + ") g(x) WHERE x >= 0");
        expect("WITH RECURSIVE c(x) AS (SELECT 0 UNION ALL SELECT x + 1 FROM c WHERE x < " + last +
               ") SELECT count(*), sum(x) FROM c");
        auto all = conn.Query("SELECT x FROM generate_series(0, " + last + ") g(x)");
        CHECK((int64_t)all.RowCount() == n);
    }
}

TEST_CASE("Chunks: a database file of 2047 to 4097 rows reads back") {
    for (int64_t n : Sizes()) {
        CAPTURE(n);
        auto path = std::filesystem::temp_directory_path() /
                    ("zugodb_chunks_" + std::to_string(n) + ".db");
        std::filesystem::remove(path);
        Summary want;
        {
            Database db(path.string());
            Connection conn(db);
            MakeTables(conn, n);
            want = Table(conn, "t");
        }
        {
            Database db(path.string());
            Connection conn(db);
            CheckSame(Table(conn, "t"), want);
        }
        std::filesystem::remove(path);
    }
}

TEST_CASE("Chunks: a write past a chunk's capacity is an error, not a write past its buffers") {
    for (idx_t capacity : {VECTOR_SIZE, VECTOR_SIZE * 2 + 1}) {
        CAPTURE(capacity);
        DataChunk chunk;
        chunk.Initialize({LogicalType::BIGINT(), LogicalType::VARCHAR()}, capacity);
        // The last row of the capacity, and the count of all of them.
        chunk.SetValue(0, capacity - 1, Value::BIGINT(1));
        chunk.SetValue(1, capacity - 1, Value());
        chunk.SetCardinality(capacity - 1);
        chunk.SetCardinality(capacity);
        CHECK(chunk.size() == capacity);
        CHECK(chunk.GetValue(1, capacity - 1).IsNull());
        // One past it.
        CHECK_THROWS_AS(chunk.SetValue(0, capacity, Value::BIGINT(1)), InternalException);
        CHECK_THROWS_AS(chunk.SetValue(1, capacity, Value()), InternalException);
        CHECK_THROWS_AS(chunk.SetCardinality(capacity + 1), InternalException);
        CHECK(chunk.size() == capacity);
        Vector vec(LogicalType::INTEGER(), capacity);
        vec.SetValue(capacity - 1, Value::INTEGER(7));
        CHECK(vec.GetValue(capacity - 1).GetValue<int32_t>() == 7);
        CHECK_THROWS_AS(vec.SetValue(capacity, Value::INTEGER(7)), InternalException);
    }
    // A chunk of no columns has no buffer to write past.
    DataChunk empty;
    empty.Initialize({});
    empty.SetCardinality(VECTOR_SIZE + 1);
    CHECK(empty.size() == VECTOR_SIZE + 1);
}
