// Integer arithmetic in the operands' own type, with DuckDB's overflow
// errors (#43). DuckDB v2.0.0 types TINYINT + TINYINT as TINYINT and
// raises "Overflow in addition of INT8 (127 + 1)!"; the engine widened
// TINYINT and SMALLINT to INTEGER and gave 128. Each case is at the limit
// of its type (DuckDB's value and type) and one past it (DuckDB's error),
// recorded with DuckDB's Python package at the corpus's tag (L7).
//
// An integer literal takes the other operand's type when it fits it, as
// DuckDB's INTEGER_LITERAL does: `x + 1` of a TINYINT column is TINYINT
// arithmetic. `/` divides as doubles and never overflows.

#include "doctest.h"
#include "../conformance_rows.hpp"
#include "slothdb/main/database.hpp"

#include <string>

using namespace zugodb;
using namespace zugodb::conformance;

namespace {

struct Width {
    const char *type;
    const char *max;      // greatest value
    const char *max_m1;   // greatest - 1
    const char *min;      // least value, as an expression of that type
    const char *min_p1;   // least + 1
};

// The least BIGINT has no literal: 9223372036854775808 is past BIGINT.
const Width kWidths[] = {
    {"TINYINT", "127", "126", "(-128)::TINYINT", "-127"},
    {"SMALLINT", "32767", "32766", "(-32768)::SMALLINT", "-32767"},
    {"INTEGER", "2147483647", "2147483646", "(-2147483648)::INTEGER", "-2147483647"},
    {"BIGINT", "9223372036854775807", "9223372036854775806",
     "(-9223372036854775807 - 1)::BIGINT", "-9223372036854775807"},
};

std::string Typed(const char *v, const char *type) {
    return std::string("(") + v + ")::" + type;
}

// One value and its type name.
std::pair<std::string, std::string> One(Connection &conn, const std::string &sql) {
    auto r = conn.Query(sql);
    REQUIRE(r.RowCount() == 1);
    REQUIRE(r.ColumnCount() == 1);
    auto v = r.GetValue(0, 0);
    return {v.IsNull() ? "NULL" : v.ToString(), r.column_types[0].ToString()};
}

} // namespace

TEST_CASE("#43: + at each integer type's limit, and one past it") {
    Database db;
    Connection conn(db);
    for (auto &w : kWidths) {
        CAPTURE(w.type);
        // The greatest value, then one past it, from both sides.
        CHECK(One(conn, "SELECT " + Typed(w.max_m1, w.type) + " + " + Typed("1", w.type)) ==
              std::make_pair(std::string(w.max), std::string(w.type)));
        CHECK_THROWS(conn.Query("SELECT " + Typed(w.max, w.type) + " + " + Typed("1", w.type)));
        CHECK_THROWS(conn.Query("SELECT " + Typed("1", w.type) + " + " + Typed(w.max, w.type)));
        // The least value, then one below it.
        CHECK(One(conn, "SELECT " + Typed(w.min_p1, w.type) + " + " + Typed("-1", w.type)).second ==
              w.type);
        CHECK_THROWS(conn.Query("SELECT " + std::string(w.min) + " + " + Typed("-1", w.type)));
        // A literal takes the column's type: no widening.
        CHECK(One(conn, "SELECT " + Typed(w.max_m1, w.type) + " + 1") ==
              std::make_pair(std::string(w.max), std::string(w.type)));
        CHECK_THROWS(conn.Query("SELECT " + Typed(w.max, w.type) + " + 1"));
        CHECK_THROWS(conn.Query("SELECT 1 + " + Typed(w.max, w.type)));
    }
    // Mixed widths compute in the wider type (DuckDB: SMALLINT 200).
    CHECK(One(conn, "SELECT 100::TINYINT + 100::SMALLINT") ==
          std::make_pair(std::string("200"), std::string("SMALLINT")));
    CHECK_THROWS(conn.Query("SELECT 1::TINYINT + 32767::SMALLINT"));
    // A literal that does not fit takes its own type (DuckDB: INTEGER).
    CHECK(One(conn, "SELECT 100::TINYINT + 1000") ==
          std::make_pair(std::string("1100"), std::string("INTEGER")));
    // An expression is not a literal (DuckDB: INTEGER 128).
    CHECK(One(conn, "SELECT 127::TINYINT + (1 + 0)") ==
          std::make_pair(std::string("128"), std::string("INTEGER")));
    // A NULL takes the other operand's type.
    CHECK(One(conn, "SELECT 1::TINYINT + NULL").first == "NULL");
}

TEST_CASE("#43: - at each integer type's limit, and one past it") {
    Database db;
    Connection conn(db);
    for (auto &w : kWidths) {
        CAPTURE(w.type);
        CHECK(One(conn, "SELECT " + Typed(w.min_p1, w.type) + " - " + Typed("1", w.type)).second ==
              w.type);
        CHECK_THROWS(conn.Query("SELECT " + std::string(w.min) + " - " + Typed("1", w.type)));
        CHECK(One(conn, "SELECT " + Typed(w.max_m1, w.type) + " - " + Typed("-1", w.type)) ==
              std::make_pair(std::string(w.max), std::string(w.type)));
        CHECK_THROWS(conn.Query("SELECT " + Typed(w.max, w.type) + " - " + Typed("-1", w.type)));
        CHECK_THROWS(conn.Query("SELECT " + std::string(w.min) + " - 1"));
        CHECK_THROWS(conn.Query("SELECT " + Typed(w.max, w.type) + " - (-1)"));
    }
    // DuckDB: -2147483648 is an INTEGER literal, so this is INTEGER
    // arithmetic and overflows; it was a BIGINT -2147483649.
    CHECK(One(conn, "SELECT -2147483647 - 1") ==
          std::make_pair(std::string("-2147483648"), std::string("INTEGER")));
    CHECK_THROWS(conn.Query("SELECT -2147483648 - 1"));
}

TEST_CASE("#43: * at each integer type's limit, and one past it") {
    Database db;
    Connection conn(db);
    for (auto &w : kWidths) {
        CAPTURE(w.type);
        CHECK(One(conn, "SELECT " + Typed(w.max, w.type) + " * " + Typed("1", w.type)) ==
              std::make_pair(std::string(w.max), std::string(w.type)));
        CHECK(One(conn, "SELECT " + std::string(w.min) + " * " + Typed("1", w.type)).second ==
              w.type);
        CHECK(One(conn, "SELECT " + Typed(w.min_p1, w.type) + " * " + Typed("-1", w.type)) ==
              std::make_pair(std::string(w.max), std::string(w.type)));
        CHECK_THROWS(conn.Query("SELECT " + Typed(w.max, w.type) + " * " + Typed("2", w.type)));
        CHECK_THROWS(conn.Query("SELECT " + std::string(w.min) + " * " + Typed("-1", w.type)));
        CHECK_THROWS(conn.Query("SELECT " + std::string(w.min) + " * " + Typed("2", w.type)));
        CHECK_THROWS(conn.Query("SELECT " + Typed("-1", w.type) + " * " + std::string(w.min)));
        CHECK_THROWS(conn.Query("SELECT 2 * " + Typed(w.max, w.type)));
    }
    // The halves of the range: the last product that fits each sign.
    CHECK(One(conn, "SELECT (-64)::TINYINT * 2::TINYINT").first == "-128");
    CHECK_THROWS(conn.Query("SELECT (-65)::TINYINT * 2::TINYINT"));
    CHECK(One(conn, "SELECT (-63)::TINYINT * (-2)::TINYINT").first == "126");
    CHECK_THROWS(conn.Query("SELECT (-64)::TINYINT * (-2)::TINYINT"));
    CHECK(One(conn, "SELECT 4611686018427387903 * 2").first == "9223372036854775806");
    CHECK_THROWS(conn.Query("SELECT 4611686018427387904 * 2"));
    // DuckDB: TINYINT * 1000 is INTEGER, as 1000 is past TINYINT.
    CHECK(One(conn, "SELECT 127::TINYINT * 1000") ==
          std::make_pair(std::string("127000"), std::string("INTEGER")));
}

TEST_CASE("#43: / of integers is a DOUBLE and does not overflow") {
    Database db;
    Connection conn(db);
    // DuckDB: (-128)::TINYINT / (-1)::TINYINT is 128.0, a DOUBLE (Value::ToString
    // prints it 128).
    CHECK(One(conn, "SELECT (-128)::TINYINT / (-1)::TINYINT") ==
          std::make_pair(std::string("128"), std::string("DOUBLE")));
    CHECK(One(conn, "SELECT (-32768)::SMALLINT / (-1)::SMALLINT") ==
          std::make_pair(std::string("32768"), std::string("DOUBLE")));
    CHECK(One(conn, "SELECT (-2147483648)::INTEGER / (-1)::INTEGER") ==
          std::make_pair(std::string("2147483648"), std::string("DOUBLE")));
    CHECK(One(conn, "SELECT 7::TINYINT / 2::TINYINT") ==
          std::make_pair(std::string("3.5"), std::string("DOUBLE")));
}

TEST_CASE("#43: % at each integer type's limit, and one past it") {
    Database db;
    Connection conn(db);
    for (auto &w : kWidths) {
        CAPTURE(w.type);
        CHECK(One(conn, "SELECT " + Typed(w.min_p1, w.type) + " % " + Typed("-1", w.type)) ==
              std::make_pair(std::string("0"), std::string(w.type)));
        CHECK(One(conn, "SELECT " + std::string(w.min) + " % " + Typed("2", w.type)) ==
              std::make_pair(std::string("0"), std::string(w.type)));
        // DuckDB: "Overflow in division of -128 / -1"; it gave 0.
        CHECK_THROWS(conn.Query("SELECT " + std::string(w.min) + " % " + Typed("-1", w.type)));
        CHECK_THROWS(conn.Query("SELECT " + Typed("5", w.type) + " % " + Typed("0", w.type)));
    }
    CHECK(One(conn, "SELECT (-7)::TINYINT % 2").first == "-1");
}

TEST_CASE("#43: unary minus at each integer type's limit, and one past it") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t(a TINYINT, b SMALLINT, c INTEGER, d BIGINT)");
    conn.Query("INSERT INTO t VALUES (-127, -32767, -2147483647, -9223372036854775807)");
    CHECK(Run(conn, "SELECT -a, -b, -c, -d FROM t") ==
          Rows{{"127", "32767", "2147483647", "9223372036854775807"}});
    auto r = conn.Query("SELECT -a, -b, -c, -d FROM t");
    CHECK(r.column_types[0].ToString() == "TINYINT");
    CHECK(r.column_types[1].ToString() == "SMALLINT");
    conn.Query("UPDATE t SET a = a - 1, b = b - 1, c = c - 1, d = d - 1");
    // DuckDB: "Overflow in negation of numeric value!"; each wrapped to
    // itself (TINYINT had no negation at all).
    CHECK_THROWS(conn.Query("SELECT -a FROM t"));
    CHECK_THROWS(conn.Query("SELECT -b FROM t"));
    CHECK_THROWS(conn.Query("SELECT -c FROM t"));
    CHECK_THROWS(conn.Query("SELECT -d FROM t"));
    for (auto &w : kWidths) {
        CAPTURE(w.type);
        CHECK(One(conn, "SELECT -(" + Typed(w.min_p1, w.type) + ")") ==
              std::make_pair(std::string(w.max), std::string(w.type)));
    }
}

TEST_CASE("#43: a column's arithmetic, and SUM over it") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t(x TINYINT, y SMALLINT, i INTEGER, g INTEGER)");
    conn.Query("INSERT INTO t VALUES (127, 32767, 2147483647, 1)");
    CHECK_THROWS(conn.Query("SELECT x + 1 FROM t"));
    CHECK_THROWS(conn.Query("SELECT x + y FROM t"));
    CHECK_THROWS(conn.Query("SELECT y + 1 FROM t"));
    CHECK(Run(conn, "SELECT x + 1::BIGINT, x * 1000, x - 1 FROM t") ==
          Rows{{"128", "127000", "126"}});
    // SUM(i + 1) was summed as SUM(i) + COUNT * 1, which never saw the
    // row that overflows: 2147483648, where DuckDB raises.
    CHECK_THROWS(conn.Query("SELECT SUM(i + 1) FROM t"));
    CHECK_THROWS(conn.Query("SELECT g, SUM(i + 1) FROM t GROUP BY g"));
    CHECK_THROWS(conn.Query("SELECT SUM(x + 1) FROM t"));
    CHECK(Run(conn, "SELECT SUM(i - 1), SUM(x - 1) FROM t") == Rows{{"2147483646", "126"}});
}

TEST_CASE("#43: shifts and bitwise operators in the operands' type") {
    Database db;
    Connection conn(db);
    // DuckDB v2.0.0's results and errors.
    CHECK(One(conn, "SELECT 1::TINYINT << 6") ==
          std::make_pair(std::string("64"), std::string("TINYINT")));
    CHECK_THROWS(conn.Query("SELECT 1::TINYINT << 7"));
    CHECK_THROWS(conn.Query("SELECT 1::TINYINT << 8"));
    CHECK(One(conn, "SELECT 1::SMALLINT << 14").first == "16384");
    CHECK_THROWS(conn.Query("SELECT 1::SMALLINT << 15"));
    CHECK(One(conn, "SELECT 1 << 30").first == "1073741824");
    CHECK_THROWS(conn.Query("SELECT 1 << 31"));
    CHECK_THROWS(conn.Query("SELECT 1 << 32"));
    CHECK(One(conn, "SELECT 1::BIGINT << 62").first == "4611686018427387904");
    CHECK_THROWS(conn.Query("SELECT 1::BIGINT << 63"));
    CHECK_THROWS(conn.Query("SELECT -1 << 1"));
    CHECK_THROWS(conn.Query("SELECT 1 << -1"));
    CHECK(Run(conn, "SELECT 0 << 100, 8 >> 1, 8 >> 40, -8 >> 1, -8 >> 40, 8 >> -1") ==
          Rows{{"0", "4", "0", "-4", "0", "0"}});
    CHECK(One(conn, "SELECT (-128)::TINYINT >> 7") ==
          std::make_pair(std::string("-1"), std::string("TINYINT")));
    CHECK(One(conn, "SELECT 5::TINYINT & 3") ==
          std::make_pair(std::string("1"), std::string("TINYINT")));
    CHECK(One(conn, "SELECT 127::TINYINT | 1000") ==
          std::make_pair(std::string("1023"), std::string("INTEGER")));
}

TEST_CASE("#43: TRUNC, CEIL, ROUND and ABS of every integer width") {
    Database db;
    Connection conn(db);
    // function/numeric/test_trunc.test: DuckDB gives 54 and -27. TRUNC
    // read every argument but a DOUBLE as a double's bytes, and gave 0;
    // CEIL, FLOOR and ROUND gave 0 for TINYINT and SMALLINT.
    CHECK(Run(conn, "SELECT trunc(54::TINYINT), trunc((-27)::SMALLINT), trunc(-27::INTEGER), "
                    "trunc(-27::BIGINT), trunc(-42.5::FLOAT)") ==
          Rows{{"54", "-27", "-27", "-27", "-42"}});
    CHECK(Run(conn, "SELECT ceil(2::TINYINT), floor((-2)::SMALLINT), round(3::TINYINT)") ==
          Rows{{"2", "-2", "3"}});
    // DuckDB: "Overflow on abs(-128)"; it was NULL.
    CHECK(Run(conn, "SELECT abs((-127)::TINYINT)") == Rows{{"127"}});
    CHECK_THROWS(conn.Query("SELECT abs((-128)::TINYINT)"));
    CHECK_THROWS(conn.Query("SELECT abs((-32768)::SMALLINT)"));
    CHECK_THROWS(conn.Query("SELECT abs((-2147483648)::INTEGER)"));
    CHECK_THROWS(conn.Query("SELECT abs(-9223372036854775807 - 1)"));
}
