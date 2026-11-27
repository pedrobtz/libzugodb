// Expression depth (#65). The parser bounds an expression tree's height at
// 256 (Parser::MAX_EXPR_DEPTH): a leaf is 1 and each operator one more, a
// subquery one more than the tallest expression in it. A chain of binary
// operators is built in a loop, so before #65 the parser never saw its
// height, and the binder, planner, executor and destructors recursed as
// deep as the chain was long: `SELECT 1 + 1 + ...` with 5,000 terms
// overflowed the C stack. Each family is tested at height 256, the
// deepest accepted, at 257, refused, and at 100,000 terms, refused
// without a crash.

#include "doctest.h"
#include "../conformance_rows.hpp"
#include "zugodb/common/exception.hpp"
#include "zugodb/main/database.hpp"
#include "zugodb/parser/parser.hpp"

#include <string>

using namespace zugodb;
using namespace zugodb::conformance;

namespace {

// `first`, then `ops` times `op` and `term`: a tree of height ops + 1
// when `first` and `term` are leaves.
std::string Chain(const std::string &first, const std::string &op,
                  const std::string &term, int ops) {
    std::string s = first;
    s.reserve(first.size() + static_cast<size_t>(ops) * (op.size() + term.size() + 2));
    for (int i = 0; i < ops; i++) {
        s += " ";
        s += op;
        s += " ";
        s += term;
    }
    return s;
}

std::string Repeat(const std::string &x, int n) {
    std::string s;
    s.reserve(x.size() * static_cast<size_t>(n));
    for (int i = 0; i < n; i++) s += x;
    return s;
}

// `n` prefix operators before `operand`: a tree of height n + 1.
std::string Prefix(const std::string &op, const std::string &operand, int n) {
    return Repeat(op + " ", n) + operand;
}

void CheckRefused(Connection &conn, const std::string &sql) {
    CHECK_THROWS_AS(conn.Query(sql), ParserException);
    // The connection answers the next query.
    CHECK(Run(conn, "SELECT 1") == Rows{{"1"}});
}

} // namespace

TEST_CASE("Depth: + and * chains at 256, 257 and 100,000 terms") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT " + Chain("1", "+", "1", 255)) == Rows{{"256"}});
    CheckRefused(conn, "SELECT " + Chain("1", "+", "1", 256));
    CheckRefused(conn, "SELECT " + Chain("1", "+", "1", 100000));
    CHECK(Run(conn, "SELECT " + Chain("1", "*", "1", 255)) == Rows{{"1"}});
    CheckRefused(conn, "SELECT " + Chain("1", "*", "1", 256));
    CheckRefused(conn, "SELECT " + Chain("1", "*", "1", 100000));
}

TEST_CASE("Depth: || chains at 256, 257 and 100,000 terms") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT " + Chain("'a'", "||", "'a'", 255)) ==
          Rows{{std::string(256, 'a')}});
    CheckRefused(conn, "SELECT " + Chain("'a'", "||", "'a'", 256));
    CheckRefused(conn, "SELECT " + Chain("'a'", "||", "'a'", 100000));
}

TEST_CASE("Depth: AND and OR chains at 256, 257 and 100,000 terms") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT " + Chain("TRUE", "AND", "TRUE", 255)) == Rows{{"true"}});
    CheckRefused(conn, "SELECT " + Chain("TRUE", "AND", "TRUE", 256));
    CheckRefused(conn, "SELECT " + Chain("TRUE", "AND", "TRUE", 100000));
    CHECK(Run(conn, "SELECT " + Chain("FALSE", "OR", "FALSE", 255)) == Rows{{"false"}});
    CheckRefused(conn, "SELECT " + Chain("FALSE", "OR", "FALSE", 256));
    CheckRefused(conn, "SELECT " + Chain("FALSE", "OR", "FALSE", 100000));
    // In a WHERE clause over a table, where the executor evaluates it.
    conn.Query("CREATE TABLE t (x INTEGER)");
    conn.Query("INSERT INTO t VALUES (1), (2), (3)");
    // Each term `x > 0` is 2 high, so 254 ANDs make a tree of 256.
    CHECK(Run(conn, "SELECT count(*) FROM t WHERE " + Chain("x > 0", "AND", "x > 0", 254)) ==
          Rows{{"3"}});
    CheckRefused(conn, "SELECT count(*) FROM t WHERE " + Chain("x > 0", "AND", "x > 0", 255));
}

TEST_CASE("Depth: unary -, + and NOT at 256, 257 and 100,000") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT " + Prefix("-", "1", 255)) == Rows{{"-1"}});
    CheckRefused(conn, "SELECT " + Prefix("-", "1", 256));
    CheckRefused(conn, "SELECT " + Prefix("-", "1", 100000));
    CHECK(Run(conn, "SELECT " + Prefix("+", "1", 255)) == Rows{{"1"}});
    CheckRefused(conn, "SELECT " + Prefix("+", "1", 256));
    CheckRefused(conn, "SELECT " + Prefix("+", "1", 100000));
    CHECK(Run(conn, "SELECT " + Prefix("NOT", "TRUE", 255)) == Rows{{"false"}});
    CheckRefused(conn, "SELECT " + Prefix("NOT", "TRUE", 256));
    CheckRefused(conn, "SELECT " + Prefix("NOT", "TRUE", 100000));
}

TEST_CASE("Depth: :: cast chains at 256, 257 and 100,000") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT 7" + Repeat("::INTEGER", 255)) == Rows{{"7"}});
    CheckRefused(conn, "SELECT 7" + Repeat("::INTEGER", 256));
    CheckRefused(conn, "SELECT 7" + Repeat("::INTEGER", 100000));
}

TEST_CASE("Depth: parentheses at 255, 256 and 100,000") {
    // Parentheses make no node; they nest ParseExpression(), which the
    // same limit bounds: 255 pairs are 256 levels with the outermost.
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT " + Repeat("(", 255) + "1" + Repeat(")", 255)) == Rows{{"1"}});
    CheckRefused(conn, "SELECT " + Repeat("(", 256) + "1" + Repeat(")", 256));
    CheckRefused(conn, "SELECT " + Repeat("(", 100000) + "1" + Repeat(")", 100000));
}

TEST_CASE("Depth: a height adds up across parentheses") {
    // (1 + ... 150 ops) is 151 high; 105 more + make 256, 106 make 257.
    Database db;
    Connection conn(db);
    std::string inner = "(" + Chain("1", "+", "1", 150) + ")";
    CHECK(Run(conn, "SELECT " + Chain(inner, "+", "1", 105)) == Rows{{"256"}});
    CheckRefused(conn, "SELECT " + Chain(inner, "+", "1", 106));
    // Nested parentheses, each holding a chain: no level is past the
    // limit alone, the tree is.
    std::string nested = "1";
    for (int i = 0; i < 40; i++) nested = "(" + Chain(nested, "+", "1", 10) + ")";
    CheckRefused(conn, "SELECT " + nested);
}

TEST_CASE("Depth: a subquery is as high as the expressions in it") {
    // The inner chain is 201 high, its subquery 202; 54 more + make 256.
    Database db;
    Connection conn(db);
    std::string scalar = "(SELECT " + Chain("1", "+", "1", 200) + ")";
    CHECK(Run(conn, "SELECT " + Chain(scalar, "+", "1", 54)) == Rows{{"255"}});
    CheckRefused(conn, "SELECT " + Chain(scalar, "+", "1", 55));
    // The tallest expression anywhere in the subquery counts: here in its
    // WHERE clause, under a FROM subquery of its own.
    std::string exists = "EXISTS (SELECT 1 FROM (SELECT 1 AS y) s WHERE " +
                         Chain("TRUE", "AND", "TRUE", 200) + ")";
    CHECK(Run(conn, "SELECT " + Chain(exists, "AND", "TRUE", 54)) == Rows{{"true"}});
    CheckRefused(conn, "SELECT " + Chain(exists, "AND", "TRUE", 55));
    // IN: the taller of its left side and its subquery.
    std::string in = Chain("1", "+", "0", 200) + " IN (SELECT 1)";
    CHECK(Run(conn, "SELECT " + Chain("(" + in + ")", "OR", "FALSE", 54)) == Rows{{"true"}});
    CheckRefused(conn, "SELECT " + Chain("(" + in + ")", "OR", "FALSE", 55));
    // Subqueries nested in subqueries, none past the limit alone.
    std::string deep = "1";
    for (int i = 0; i < 40; i++) deep = "(SELECT " + Chain(deep, "+", "1", 10) + ")";
    CheckRefused(conn, "SELECT " + deep);
}

TEST_CASE("Depth: a window is as high as its arguments, partition and order") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE t (x INTEGER)");
    conn.Query("INSERT INTO t VALUES (1), (2)");
    // PARTITION BY a chain 255 high: the window is 256.
    CHECK(Run(conn, "SELECT row_number() OVER (PARTITION BY " + Chain("x", "+", "0", 254) +
                        " ORDER BY x) AS r FROM t ORDER BY x") == Rows{{"1"}, {"1"}});
    CheckRefused(conn, "SELECT row_number() OVER (PARTITION BY " + Chain("x", "+", "0", 255) +
                           " ORDER BY x) AS r FROM t");
    CheckRefused(conn, "SELECT row_number() OVER (ORDER BY " + Chain("x", "+", "0", 255) +
                           ") AS r FROM t");
    CheckRefused(conn, "SELECT sum(" + Chain("x", "+", "0", 255) + ") OVER () AS r FROM t");
}

TEST_CASE("Depth: wide expressions are not deep") {
    // A long IN list, CASE, function argument list or select list is one
    // level, however long.
    Database db;
    Connection conn(db);
    std::string in_list = "1";
    for (int i = 2; i <= 10000; i++) in_list += ", " + std::to_string(i);
    CHECK(Run(conn, "SELECT 10000 IN (" + in_list + ")") == Rows{{"true"}});
    std::string when;
    for (int i = 1; i <= 5000; i++)
        when += " WHEN 5000 = " + std::to_string(i) + " THEN " + std::to_string(i);
    CHECK(Run(conn, "SELECT CASE" + when + " END") == Rows{{"5000"}});
    std::string args = "NULL";
    for (int i = 0; i < 5000; i++) args += ", NULL";
    CHECK(Run(conn, "SELECT coalesce(" + args + ", 3)") == Rows{{"3"}});
}

TEST_CASE("Depth: a chain is refused as it is built, not after") {
    // Each loop checks the node it builds, so a chain is refused one level
    // past the limit: a tree of 2,000,000 levels, built whole and then
    // refused, overflowed the stack in its own destructor.
    Database db;
    Connection conn(db);
    CheckRefused(conn, "SELECT " + Chain("1", "+", "1", 2000000));
    CheckRefused(conn, "SELECT " + Chain("TRUE", "AND", "TRUE", 2000000));
}
