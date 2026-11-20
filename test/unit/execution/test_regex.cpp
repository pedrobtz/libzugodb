// The regular expressions of the regexp_ functions (libzugodb Stage 8, #32;
// design L24): DuckDB's recorded answers from the corpus files named
// (test/conformance/corpus/test/sql/function/string/...), the refusals of
// what the RE2 subset does not hold, and a differential test of the Pike
// VM against std::regex over patterns where ECMAScript and RE2 agree.

#include "doctest.h"
#include "../conformance_rows.hpp"
#include "slothdb/execution/regex.hpp"
#include "slothdb/main/database.hpp"

#include <regex>
#include <string>
#include <vector>

using namespace zugodb;
using namespace zugodb::conformance;

TEST_CASE("Stage 8: regexp_extract's group, default and options") {
    Database db;
    Connection conn(db);
    // regex_extract.test
    CHECK(Run(conn, "SELECT regexp_extract('foobarbaz', 'b..'), regexp_extract('foobarbaz', 'B..'), "
                    "regexp_extract('foobarbaz', 'B..', 0, 'i'), regexp_extract('foobarbaz', 'b..', 1)") ==
          Rows{{"bar", "", "bar", ""}});
    CHECK(Run(conn, "SELECT regexp_extract('foobarbaz', '(b..)(b..)'), "
                    "regexp_extract('foobarbaz', '(b..)(b..)', 1), "
                    "regexp_extract('foobarbaz', '(b..)(b..)', 2)") ==
          Rows{{"barbaz", "bar", "baz"}});
    CHECK(Run(conn, "SELECT regexp_extract('I am the DUCKIEST duck to ever duck', 'duck', 'il'), "
                    "regexp_extract('bibitybobbitybarbazity', '(b..)(b..)', 'i')") ==
          Rows{{"DUCK", "bobbit"}});
    CHECK_THROWS(conn.Query("SELECT regexp_extract('foobarbaz', '(b..)(b..)', -1)"));
    CHECK_THROWS(conn.Query("SELECT regexp_extract('foobarbaz', '(b..)(b..)', 42)"));
    CHECK_THROWS(conn.Query("SELECT regexp_extract('to quack or not to quack', 'lalalala', 'g')"));
    CHECK_THROWS(conn.Query("SELECT regexp_extract('abcdefg', 'A..', 'i', 'c')"));
    // A 'k' option and any other letter DuckDB has and the engine not.
    CHECK_THROWS(conn.Query("SELECT regexp_extract('foobarbaz', '(zzz)', 1, 'k')"));
}

TEST_CASE("Stage 8: regexp_replace replaces the first match unless 'g'") {
    Database db;
    Connection conn(db);
    // regex_replace.test
    CHECK(Run(conn, "SELECT regexp_replace('foobarbaz', 'b..', 'X'), "
                    "regexp_replace('ana ana', 'ana', 'banana', 'g'), "
                    "regexp_replace('ANA ana', 'ana', 'banana', 'gi'), "
                    "regexp_replace('ANA', 'ana', 'banana', 'i'), "
                    "regexp_replace('as^/$df', '^/$', '', 'l'), regexp_replace('as^/$df', '^/$', '')") ==
          Rows{{"fooXbaz", "banana banana", "banana banana", "banana", "asdf", "as^/$df"}});
    CHECK(Run(conn, "SELECT regexp_replace('hello\nworld', '.*', 'x', 'sg')") == Rows{{"x"}});
    conn.Query("CREATE TABLE test(v VARCHAR)");
    conn.Query("INSERT INTO test VALUES ('hello'), ('HELLO')");
    CHECK(Run(conn, "SELECT regexp_replace(v, 'h.*', 'world', 'i') FROM test ORDER BY v") ==
          Rows{{"world"}, {"world"}});
    CHECK_THROWS(conn.Query("SELECT regexp_replace(v, 'h.*', 'world', v) FROM test"));
    // RE2's rewrite: \0 to \9 and \\, an empty match where the last ended
    // skipped.
    CHECK(Run(conn, "SELECT regexp_replace('abc', '(b)', '[\\1]'), regexp_replace('abc', 'x*', '-', 'g'), "
                    "regexp_replace('hello', '.*', 'x', 'g')") ==
          Rows{{"a[b]c", "-a-b-c-", "x"}});
    CHECK_THROWS(conn.Query("SELECT regexp_replace('abc', 'b', '\\2')"));
    CHECK_THROWS(conn.Query("SELECT regexp_replace('abc', 'b', 'X\\xY')"));
}

TEST_CASE("Stage 8: regexp_matches and regexp_full_match") {
    Database db;
    Connection conn(db);
    // regex_search.test
    CHECK(Run(conn, "SELECT regexp_matches('asdf', '.*sd.*'), regexp_matches('asdf', ''), "
                    "regexp_full_match('asdf', 'sd'), regexp_full_match('asdf', '.sd.'), "
                    "regexp_matches('asdf', '^sdf$'), regexp_matches('', '.*'), "
                    "regexp_matches('asdf', '.*SD.*', ' i \t'), regexp_matches('as^/$df', '^/$', 'l')") ==
          Rows{{"true", "true", "false", "true", "false", "true", "true", "true"}});
    CHECK(Run(conn, "SELECT regexp_matches('hello\nworld', '.*', 's')") == Rows{{"true"}});
    CHECK_THROWS(conn.Query("SELECT regexp_matches('', '\\X')"));
    CHECK_THROWS(conn.Query("SELECT regexp_matches('asdf', '.*SD.*', 'q')"));
    CHECK_THROWS(conn.Query("SELECT regexp_matches('asdf', '.*SD.*', 'g')"));
    CHECK_THROWS(conn.Query("SELECT regexp_matches('asdf', '.*SD.*', NULL)"));
}

TEST_CASE("Stage 8: what RE2 refuses, and what the subset does not hold, is an error") {
    Database db;
    Connection conn(db);
    const char *queries[] = {
        // test_regex_match_positions.test, regex_single_byte.test,
        // regexp_unicode_literal.test
        "SELECT regexp_extract('abc', p) FROM (VALUES ('(a')) t(p)",
        "SELECT regexp_replace('ä', '\\C', 'x')",
        "SELECT regexp_matches('00001', '^00\\x{FFFFFFFF}001$')",
        // Unsupported: backreferences, lookaround, inline flags, a repeat of
        // what can match empty, a literal {, a class starting with ].
        "SELECT regexp_matches('aa', '(a)\\1')",
        "SELECT regexp_matches('ab', 'a(?=b)')",
        "SELECT regexp_matches('A', '(?i)a')",
        "SELECT regexp_matches('a', '(a*)*')",
        "SELECT regexp_matches('a{', 'a{')",
        "SELECT regexp_matches(']', '[]a]')",
        "SELECT regexp_matches('a', 'a**')",
        // Where the subset cannot say what RE2 would: 'i' over non-ASCII
        // text, . over a newline without 's'.
        "SELECT regexp_matches('Ä', 'ä', 'i')",
        "SELECT regexp_matches('a\nb', 'a.b')",
        // Not DuckDB's names.
        "SELECT regexp_like('a', 'a')",
    };
    for (auto *q : queries) {
        CAPTURE(q);
        CHECK_THROWS(conn.Query(q));
    }
    // Code points, not bytes.
    CHECK(Run(conn, "SELECT regexp_extract('añb', 'a.b'), regexp_extract('ünî', '[^n]+')") ==
          Rows{{"añb", "ü"}});
}

namespace {

// A deterministic generator (the engine calls no rand; neither does this).
struct Lcg {
    uint64_t s = 0x9E3779B97F4A7C15ull;
    uint32_t Next(uint32_t n) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<uint32_t>((s >> 33) % n);
    }
};

// Patterns of the constructs on which ECMAScript and RE2 agree, over the
// alphabet {a, b, c}: no capture under a repeat is compared, only the
// match's span.
std::string Atom(Lcg &g, int depth) {
    switch (g.Next(depth > 2 ? 4 : 7)) {
    case 0: return "a";
    case 1: return "b";
    case 2: return ".";
    case 3: return g.Next(2) ? "[ab]" : "[^a]";
    case 4: return "(" + std::string(g.Next(2) ? "?:" : "") + "a" + std::string(g.Next(2) ? "|b" : "c") + ")";
    case 5: return "\\w";
    default: return "(?:b" + Atom(g, depth + 1) + ")";
    }
}

std::string Pattern(Lcg &g) {
    std::string p;
    if (g.Next(5) == 0) p += "^";
    int n = 1 + static_cast<int>(g.Next(4));
    for (int i = 0; i < n; i++) {
        p += Atom(g, 0);
        switch (g.Next(6)) {
        case 0: p += "*"; break;
        case 1: p += "+"; break;
        case 2: p += "?"; break;
        case 3: p += g.Next(2) ? "*?" : "{1,2}"; break;
        default: break;
        }
        if (g.Next(8) == 0 && i + 1 < n) p += "|";
    }
    if (g.Next(5) == 0) p += "$";
    return p;
}

} // namespace

TEST_CASE("Stage 8: the Pike VM agrees with std::regex where ECMAScript and RE2 do") {
    Lcg g;
    int compared = 0;
    for (int k = 0; k < 4000; k++) {
        std::string pat = Pattern(g);
        std::string subject;
        int len = static_cast<int>(g.Next(9));
        for (int i = 0; i < len; i++) subject += "abc"[g.Next(3)];
        CAPTURE(pat);
        CAPTURE(subject);
        regex::Regex re(pat, regex::Options{});
        std::vector<std::pair<int64_t, int64_t>> groups;
        bool ours = re.Search(subject, 0, false, false, groups);
        std::smatch m;
        bool theirs = std::regex_search(subject, m, std::regex(pat, std::regex::ECMAScript));
        REQUIRE(ours == theirs);
        if (ours) {
            CHECK(groups[0].first == m.position(0));
            CHECK(groups[0].second - groups[0].first == m.length(0));
        }
        compared++;
    }
    CHECK(compared == 4000);
}
