// Joins of the pinned DuckDB subset that gave a wrong answer (libzugodb
// Stage 8, #12). Each test runs the smallest SQL of the case's shape and
// expects DuckDB's recorded result from the corpus file it names
// (test/conformance/corpus/test/sql/...), or an error where the engine
// refuses the statement.

#include "doctest.h"
#include "../conformance_rows.hpp"
#include "slothdb/main/database.hpp"

using namespace zugodb;
using namespace zugodb::conformance;

namespace {

void MakeIntegers(Connection &conn) {
    conn.Query("CREATE TABLE integers(i INTEGER, j INTEGER)");
    conn.Query("INSERT INTO integers VALUES (1, 2), (2, 3), (3, 4)");
    conn.Query("CREATE TABLE integers2(k INTEGER, l INTEGER)");
    conn.Query("INSERT INTO integers2 VALUES (1, 10), (2, 20)");
}

} // namespace

// ---------------------------------------------------------------------------
// A join condition with no equality between a column of each side
// (join/left_outer/test_left_outer.test, join/right_outer/
// test_right_outer.test, join/test_join_always_true_conditions.test,
// join/inner/test_join.test, join/inner/test_range_join.test, ...). The
// hash join took column 0 of each side as its key and dropped the
// condition, so `ON TRUE`, `ON i = 1` or `ON i < k` joined on i = k.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: a join on a condition with no column = column equality") {
    Database db;
    Connection conn(db);
    MakeIntegers(conn);
    // test_left_outer.test, "filter on LHS"
    CHECK(Run(conn, "SELECT * FROM integers LEFT OUTER JOIN integers2 ON i=1 ORDER BY i, k") ==
          Rows{{"1", "2", "1", "10"}, {"1", "2", "2", "20"}, {"2", "3", "NULL", "NULL"},
               {"3", "4", "NULL", "NULL"}});
    // test_left_outer.test, "the 1=2 condition"
    CHECK(Run(conn, "SELECT * FROM integers LEFT OUTER JOIN integers2 ON 1=2 ORDER BY i") ==
          Rows{{"1", "2", "NULL", "NULL"}, {"2", "3", "NULL", "NULL"}, {"3", "4", "NULL", "NULL"}});
    // test_left_outer.test, "filter on RHS"
    CHECK(Run(conn, "SELECT * FROM integers LEFT OUTER JOIN integers2 ON l=20 ORDER BY i, k") ==
          Rows{{"1", "2", "2", "20"}, {"2", "3", "2", "20"}, {"3", "4", "2", "20"}});
    // test_left_outer.test, an inequality
    CHECK(Run(conn, "SELECT * FROM integers LEFT OUTER JOIN integers2 ON integers.i<integers2.k "
                    "WHERE integers.i <= 2 ORDER BY i") ==
          Rows{{"1", "2", "2", "20"}, {"2", "3", "NULL", "NULL"}});
    // test_right_outer.test
    CHECK(Run(conn, "SELECT integers.*, integers2.* FROM integers2 RIGHT OUTER JOIN integers "
                    "ON 1=2 ORDER BY i") ==
          Rows{{"1", "2", "NULL", "NULL"}, {"2", "3", "NULL", "NULL"}, {"3", "4", "NULL", "NULL"}});
    // test_join_always_true_conditions.test
    CHECK(Run(conn, "SELECT count(*) FROM integers INNER JOIN integers2 ON TRUE") == Rows{{"6"}});
    CHECK(Run(conn, "SELECT count(*) FROM integers FULL OUTER JOIN integers2 ON TRUE") == Rows{{"6"}});
    // test_range_join.test; and through the fused join + aggregate path.
    CHECK(Run(conn, "SELECT i, k FROM integers JOIN integers2 ON i < k") == Rows{{"1", "2"}});
    CHECK(Run(conn, "SELECT count(*) FROM integers JOIN integers2 ON i < k") == Rows{{"1"}});
    // An equality and a residual: the fused path took the equality alone.
    CHECK(Run(conn, "SELECT count(*) FROM integers JOIN integers2 ON i = k AND l = 20") == Rows{{"1"}});
}

TEST_CASE("Stage 8: a NULL key matches nothing, the string 'NULL' included") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE a(s VARCHAR)");
    conn.Query("INSERT INTO a VALUES ('NULL'), (NULL)");
    conn.Query("CREATE TABLE b(s VARCHAR)");
    conn.Query("INSERT INTO b VALUES (NULL), ('NULL')");
    CHECK(Run(conn, "SELECT count(*) FROM a JOIN b ON a.s = b.s") == Rows{{"1"}});
}

// ---------------------------------------------------------------------------
// NATURAL JOIN (join/natural/natural_join.test) joined on the first column
// of each side and showed the shared columns twice: refused. A join other
// than CROSS without ON is a parser error, as in DuckDB.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: NATURAL JOIN and a join without ON are refused") {
    Database db;
    Connection conn(db);
    MakeIntegers(conn);
    CHECK_THROWS(conn.Query("SELECT * FROM integers NATURAL JOIN integers2"));
    CHECK_THROWS(conn.Query("SELECT * FROM integers JOIN integers2"));
    CHECK_THROWS(conn.Query("SELECT * FROM integers LEFT JOIN integers2"));
    CHECK(Run(conn, "SELECT count(*) FROM integers CROSS JOIN integers2") == Rows{{"6"}});
}

// ---------------------------------------------------------------------------
// join/left_outer/test_left_outer.test:176 (#38): ORDER BY a position of
// SELECT * bound the column by its name, so after a join ORDER BY 1 was
// the other table's column of that name.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: ORDER BY a position of SELECT * over a join") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "select * from (values(1), (2)) t1(i) left join (values (2), (3)) t2(i) "
                    "on t1.i=t2.i AND t1.i+t2.i=4 ORDER BY 1, 2") ==
          Rows{{"1", "NULL"}, {"2", "2"}});
    conn.Query("CREATE TABLE a(i INT, j INT)");
    conn.Query("INSERT INTO a VALUES (1, 20), (2, 10)");
    CHECK(Run(conn, "SELECT i, * FROM a ORDER BY 3") == Rows{{"2", "2", "10"}, {"1", "1", "20"}});
}
