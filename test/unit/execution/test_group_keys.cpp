// Rows that differ are different groups, partitions and set members
// (libzugodb Stage 10, #47 and #48). Each query's answer is DuckDB's,
// written out.

#include "doctest.h"
#include "slothdb/main/connection.hpp"
#include "slothdb/main/database.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace zugodb;

namespace {

// The rows of a result, one string each, sorted.
std::vector<std::string> Rows(Connection &conn, const std::string &sql) {
    auto r = conn.Query(sql);
    std::vector<std::string> out;
    for (idx_t i = 0; i < r.RowCount(); i++) {
        std::string line;
        for (idx_t j = 0; j < r.ColumnCount(); j++) {
            if (j) line += '|';
            auto &v = r.GetValue(i, j);
            line += v.IsNull() ? std::string("NULL") : "'" + v.ToString() + "'";
        }
        out.push_back(std::move(line));
    }
    std::sort(out.begin(), out.end());
    return out;
}

using Want = std::vector<std::string>;

void Check(Connection &conn, const std::string &sql, Want want) {
    CAPTURE(sql);
    std::sort(want.begin(), want.end());
    CHECK(Rows(conn, sql) == want);
}

// z: a NULL key, two rows of 0 and '', the text 'NULL', and two rows whose
// texts joined with '|' are the same.
void MakeZ(Connection &conn) {
    conn.Query("CREATE TABLE z (k INTEGER, b BIGINT, s VARCHAR, s2 VARCHAR, v INTEGER)");
    conn.Query("INSERT INTO z VALUES (NULL, NULL, NULL, 'x', 1), (0, 0, '', 'x', 2), (0, 0, '', 'x', 3), "
               "(5, 5, 'NULL', 'x', 4), (6, 6, 'a|', 'b', 5), (7, 7, 'a', '|b', 6)");
}

} // namespace

TEST_CASE("Group keys: a NULL key is its own group, not 0's or ''s (#47)") {
    Database db;
    Connection conn(db);
    MakeZ(conn);
    Check(conn, "SELECT k, sum(v), count(*) FROM z GROUP BY k",
          {"NULL|'1'|'1'", "'0'|'5'|'2'", "'5'|'4'|'1'", "'6'|'5'|'1'", "'7'|'6'|'1'"});
    Check(conn, "SELECT b, max(v) FROM z GROUP BY b",
          {"NULL|'1'", "'0'|'3'", "'5'|'4'", "'6'|'5'", "'7'|'6'"});
    Check(conn, "SELECT s, sum(v) FROM z GROUP BY s",
          {"NULL|'1'", "''|'5'", "'NULL'|'4'", "'a|'|'5'", "'a'|'6'"});
    Check(conn, "SELECT s, min(v), avg(v) FROM z WHERE v > 0 GROUP BY s",
          {"NULL|'1'|'1.0'", "''|'2'|'2.5'", "'NULL'|'4'|'4.0'", "'a|'|'5'|'5.0'", "'a'|'6'|'6.0'"});
    Check(conn, "SELECT k + 0 AS g, count(*) FROM z GROUP BY g",
          {"NULL|'1'", "'0'|'2'", "'5'|'1'", "'6'|'1'", "'7'|'1'"});
    Check(conn, "SELECT k, count(*) FROM z GROUP BY k HAVING count(*) > 1", {"'0'|'2'"});
}

TEST_CASE("Group keys: rows whose text joined with '|' is the same are different groups (#48)") {
    Database db;
    Connection conn(db);
    MakeZ(conn);
    const Want pairs = {"NULL|'x'", "''|'x'", "'NULL'|'x'", "'a|'|'b'", "'a'|'|b'"};
    Check(conn, "SELECT DISTINCT s, s2 FROM z", pairs);
    Check(conn, "SELECT DISTINCT s FROM z", {"NULL", "''", "'NULL'", "'a|'", "'a'"});
    Check(conn, "SELECT s, s2, sum(v) FROM z GROUP BY s, s2",
          {"NULL|'x'|'1'", "''|'x'|'5'", "'NULL'|'x'|'4'", "'a|'|'b'|'5'", "'a'|'|b'|'6'"});
    Check(conn, "SELECT k, s, sum(v) FROM z GROUP BY k, s",
          {"NULL|NULL|'1'", "'0'|''|'5'", "'5'|'NULL'|'4'", "'6'|'a|'|'5'", "'7'|'a'|'6'"});
    // Doubles that print alike to six decimals.
    conn.Query("CREATE TABLE f (a DOUBLE, b INTEGER, v INTEGER)");
    conn.Query("INSERT INTO f VALUES (0.0000001, 1, 1), (0.0000002, 1, 2), (-0.0, 1, 3), (0.0, 1, 4)");
    Check(conn, "SELECT count(*), sum(s) FROM (SELECT a, b, sum(v) AS s FROM f GROUP BY a, b) q", {"'3'|'10'"});
    Check(conn, "SELECT sum(v) FROM f GROUP BY a, b HAVING count(*) = 2", {"'7'"});
}

TEST_CASE("Group keys: UNION, INTERSECT and EXCEPT keep each row once (#48)") {
    Database db;
    Connection conn(db);
    MakeZ(conn);
    const Want pairs = {"NULL|'x'", "''|'x'", "'NULL'|'x'", "'a|'|'b'", "'a'|'|b'"};
    Check(conn, "SELECT s FROM z UNION SELECT s FROM z", {"NULL", "''", "'NULL'", "'a|'", "'a'"});
    Check(conn, "SELECT s, s2 FROM z UNION SELECT 'a', '|b'", pairs);
    Check(conn, "SELECT s, s2 FROM z INTERSECT SELECT s, s2 FROM z", pairs);
    Check(conn, "SELECT s, s2 FROM z EXCEPT SELECT 'a', '|b'",
          {"NULL|'x'", "''|'x'", "'NULL'|'x'", "'a|'|'b'"});
    // In a derived table, which is materialised by its own path.
    Check(conn, "SELECT * FROM (SELECT s FROM z UNION SELECT s FROM z) q", {"NULL", "''", "'NULL'", "'a|'", "'a'"});
    Check(conn, "SELECT * FROM (SELECT k FROM z INTERSECT SELECT k FROM z) q",
          {"NULL", "'0'", "'5'", "'6'", "'7'"});
    Check(conn, "SELECT * FROM (SELECT k FROM z EXCEPT SELECT 5) q", {"NULL", "'0'", "'6'", "'7'"});
    Check(conn, "SELECT 1 FROM z UNION ALL SELECT 1 FROM z", Want(12, "'1'"));
}

TEST_CASE("Group keys: PARTITION BY keeps NULL, '' and 0 apart, and reads expressions (#47, #48)") {
    Database db;
    Connection conn(db);
    MakeZ(conn);
    Check(conn, "SELECT v, count(*) OVER (PARTITION BY s) FROM z",
          {"'1'|'1'", "'2'|'2'", "'3'|'2'", "'4'|'1'", "'5'|'1'", "'6'|'1'"});
    Check(conn, "SELECT v, count(*) OVER (PARTITION BY k) FROM z",
          {"'1'|'1'", "'2'|'2'", "'3'|'2'", "'4'|'1'", "'5'|'1'", "'6'|'1'"});
    Check(conn, "SELECT v, row_number() OVER (PARTITION BY s, s2 ORDER BY v) FROM z",
          {"'1'|'1'", "'2'|'1'", "'3'|'2'", "'4'|'1'", "'5'|'1'", "'6'|'1'"});
    // Two windows over different partitions take the other path.
    Check(conn, "SELECT v, row_number() OVER (PARTITION BY k ORDER BY v), count(*) OVER (PARTITION BY s) FROM z",
          {"'1'|'1'|'1'", "'2'|'1'|'2'", "'3'|'2'|'2'", "'4'|'1'|'1'", "'5'|'1'|'1'", "'6'|'1'|'1'"});
    Check(conn, "SELECT v, count(*) OVER (PARTITION BY b), row_number() OVER (PARTITION BY s ORDER BY v) FROM z",
          {"'1'|'1'|'1'", "'2'|'2'|'1'", "'3'|'2'|'2'", "'4'|'1'|'1'", "'5'|'1'|'1'", "'6'|'1'|'1'"});
    Check(conn, "SELECT v, row_number() OVER (PARTITION BY v % 2 ORDER BY v), count(*) OVER (PARTITION BY k) FROM z",
          {"'1'|'1'|'1'", "'2'|'1'|'2'", "'3'|'2'|'2'", "'4'|'2'|'1'", "'5'|'3'|'1'", "'6'|'3'|'1'"});
}
