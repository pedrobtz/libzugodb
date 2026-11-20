// BETWEEN and IN compare in one type, as DuckDB's (found with #42). The
// executor compared Values, and two of different types were neither less
// nor equal: a BIGINT, DOUBLE or TINYINT column BETWEEN 1 AND 10 was true
// of every row, IN (2, 3) of none, and a DATE BETWEEN two strings always
// true. The binder now gives the value and the list, or the bounds, one
// type, a string literal the other's. Each answer below is DuckDB
// v2.0.0's.
#include "doctest.h"

#include "../conformance_rows.hpp"
#include "zugodb/main/connection.hpp"
#include "zugodb/main/database.hpp"

using namespace zugodb;
using namespace zugodb::conformance;

TEST_CASE("BETWEEN and IN of mixed types compare in one type, as DuckDB's") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE dt(d DATE, s VARCHAR, i INTEGER, b BIGINT, f DOUBLE, t TINYINT)");
    conn.Query("INSERT INTO dt VALUES ('2024-03-01', 'b', 5, 5, 2.5, 5), "
               "('2023-01-01', 'z', 50, 50, 20.0, 50), (NULL, NULL, NULL, NULL, NULL, NULL)");
    CHECK(Run(conn, "SELECT b FROM dt WHERE b BETWEEN 1 AND 10") == Rows{{"5"}});
    CHECK(Run(conn, "SELECT f FROM dt WHERE f BETWEEN 1 AND 10") == Rows{{"2.5"}});
    CHECK(Run(conn, "SELECT t FROM dt WHERE t BETWEEN 1 AND 10") == Rows{{"5"}});
    CHECK(Run(conn, "SELECT i FROM dt WHERE i BETWEEN 1.5 AND 10") == Rows{{"5"}});
    CHECK(Run(conn, "SELECT d FROM dt WHERE d BETWEEN '2024-01-01' AND '2024-12-31'") ==
          Rows{{"2024-03-01"}});
    CHECK(Run(conn, "SELECT s FROM dt WHERE s BETWEEN 'a' AND 'c'") == Rows{{"b"}});
    CHECK(Run(conn, "SELECT b FROM dt WHERE b NOT BETWEEN 1 AND 10") == Rows{{"50"}});
    CHECK(Run(conn, "SELECT b FROM dt WHERE b IN (5, 6)") == Rows{{"5"}});
    CHECK(Run(conn, "SELECT f FROM dt WHERE f IN (2.5, 3)") == Rows{{"2.5"}});
    CHECK(Run(conn, "SELECT f FROM dt WHERE f IN (20, 3)") == Rows{{"20.0"}});
    CHECK(Run(conn, "SELECT t FROM dt WHERE t IN (50, 1000)") == Rows{{"50"}});
    CHECK(Run(conn, "SELECT d FROM dt WHERE d IN ('2024-03-01')") == Rows{{"2024-03-01"}});
    CHECK(Run(conn, "SELECT i FROM dt WHERE i IN (5.0, 7)") == Rows{{"5"}});
    CHECK(Run(conn, "SELECT i FROM dt WHERE i NOT IN (5, 7)") == Rows{{"50"}});
    CHECK(Run(conn, "SELECT b BETWEEN 1 AND 10 FROM dt ORDER BY b") ==
          Rows{{"true"}, {"false"}, {"NULL"}});
    // BETWEEN is two comparisons ANDed: a NULL bound gives false where the
    // other excludes the value. It was NULL whenever any bound was.
    CHECK(Run(conn, "SELECT 5 BETWEEN NULL AND 3, 5 BETWEEN NULL AND 6, 5 BETWEEN 6 AND NULL, "
                    "NULL BETWEEN 1 AND 2") == Rows{{"false", "NULL", "false", "NULL"}});
    CHECK(Run(conn, "SELECT 1 IN (NULL, 1), 2 IN (NULL, 1), NULL IN (1)") ==
          Rows{{"true", "NULL", "NULL"}});
    // A VARCHAR column beside a number: DuckDB casts the column and fails
    // on text that is no number; refused up front.
    CHECK_THROWS(conn.Query("SELECT s FROM dt WHERE s IN (1)"));
    CHECK_THROWS(conn.Query("SELECT s FROM dt WHERE s BETWEEN 1 AND 2"));
}
