// Host-supplied function providers (Stage 11.1, #40; design L25, L29).
//
// The stubs of stub_providers.hpp stand in for zugodb's adapters over
// zuregex and zuutf: what is tested here is the plumbing (the checks of a
// table, the refusals without one, a constant pattern compiled once, match
// state per call, every error status a query error, threads), not a regex
// dialect, which zugodb tests with the real tables.

#include "doctest.h"
#include "../stub_providers.hpp"
#include "zugodb/common/exception.hpp"
#include "zugodb/common/parallel.hpp"
#include "zugodb/main/connection.hpp"
#include "zugodb/main/database.hpp"
#include "zugodb/main/provider_set.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

using namespace zugodb;

namespace {

// The error of `sql`, or "" when it runs.
std::string ErrorOf(Connection &conn, const std::string &sql) {
    try {
        conn.Query(sql);
    } catch (const std::exception &e) {
        return e.what();
    }
    return "";
}

std::string One(Connection &conn, const std::string &sql) {
    auto r = conn.Query(sql);
    REQUIRE(r.RowCount() == 1);
    REQUIRE(r.ColumnCount() == 1);
    auto &v = r.GetValue(0, 0);
    return v.IsNull() ? std::string("NULL") : v.ToString();
}

// Each regex form a provider serves, and two it does not.
const std::vector<std::string> &RegexQueries() {
    static const std::vector<std::string> q = {
        "SELECT regexp_matches('abc', 'b')",
        "SELECT regexp_full_match('abc', 'abc')",
        "SELECT regexp_replace('abc', 'b', 'x')",
        "SELECT regexp_extract('abc', 'b')",
        "SELECT 'abc' SIMILAR TO 'abc'",
        "SELECT 'abc' NOT SIMILAR TO 'abc'",
        "SELECT 'abc' ~ 'abc'",
        "SELECT regexp_split_to_array('abc', 'b')",
    };
    return q;
}

// The errors of RegexQueries() on a database with no provider.
std::vector<std::string> RefusalsWithout() {
    Database db;
    Connection conn(db);
    std::vector<std::string> out;
    for (auto &q : RegexQueries()) out.push_back(ErrorOf(conn, q));
    return out;
}

void CheckRefusedAsWithout(Database &db) {
    Connection conn(db);
    auto expected = RefusalsWithout();
    for (size_t i = 0; i < RegexQueries().size(); i++) {
        INFO(RegexQueries()[i]);
        auto got = ErrorOf(conn, RegexQueries()[i]);
        CHECK(!got.empty());
        CHECK(got.find("libzugodb#40") != std::string::npos);
        CHECK(got == expected[i]);
    }
}

struct StubDb {
    zugodb_regex_v1 regex = stub::RegexTable();
    zugodb_utf8_v1 utf8 = stub::Utf8Table();
    Database db;
    StubDb() {
        auto p = stub::Set(&regex, &utf8);
        db.SetProviders(&p);
    }
};

} // namespace

// ---------------------------------------------------------------------------
// Tables that are absent or cannot be used.
// ---------------------------------------------------------------------------

TEST_CASE("Providers - no table: the regex functions are refused, naming #40") {
    Database db;
    CHECK(db.GetProviderState(ProviderService::REGEX) == ProviderState::ABSENT);
    CheckRefusedAsWithout(db);
}

TEST_CASE("Providers - a NULL regex table: refused with the same error as none") {
    Database db;
    auto p = stub::Set(nullptr, nullptr);
    db.SetProviders(&p);
    CHECK(db.GetProviderState(ProviderService::REGEX) == ProviderState::ABSENT);
    CHECK(db.GetProviderState(ProviderService::UTF8) == ProviderState::ABSENT);
    CheckRefusedAsWithout(db);
}

TEST_CASE("Providers - a regex table of another ABI version: refused as with none") {
    auto t = stub::RegexTable();
    for (uint32_t v : {0u, 2u, 0xFFFFFFFFu}) {
        INFO("abi_version " << v);
        t.abi_version = v;
        Database db;
        auto p = stub::Set(&t, nullptr);
        db.SetProviders(&p);
        CHECK(db.GetProviderState(ProviderService::REGEX) == ProviderState::WRONG_VERSION);
        CheckRefusedAsWithout(db);
    }
}

TEST_CASE("Providers - a regex table with too small a struct_size: refused as with none") {
    auto t = stub::RegexTable();
    for (uint32_t size : {0u, 8u, (uint32_t)sizeof(zugodb_regex_v1) - 1}) {
        INFO("struct_size " << size);
        t.struct_size = size;
        Database db;
        auto p = stub::Set(&t, nullptr);
        db.SetProviders(&p);
        CHECK(db.GetProviderState(ProviderService::REGEX) == ProviderState::TOO_SMALL);
        CheckRefusedAsWithout(db);
    }
}

TEST_CASE("Providers - a regex table with a NULL member: refused as with none") {
    for (int member = 0; member < 9; member++) {
        INFO("member " << member);
        auto t = stub::RegexTable();
        switch (member) {
        case 0: t.compile = nullptr; break;
        case 1: t.pattern_free = nullptr; break;
        case 2: t.group_count = nullptr; break;
        case 3: t.match_create = nullptr; break;
        case 4: t.match_free = nullptr; break;
        case 5: t.search = nullptr; break;
        case 6: t.group = nullptr; break;
        case 7: t.replace = nullptr; break;
        default: t.strerror = nullptr; break;
        }
        Database db;
        auto p = stub::Set(&t, nullptr);
        db.SetProviders(&p);
        CHECK(db.GetProviderState(ProviderService::REGEX) == ProviderState::NULL_MEMBER);
        CheckRefusedAsWithout(db);
    }
}

TEST_CASE("Providers - a larger struct_size is a later minor version, and used") {
    auto t = stub::RegexTable();
    t.struct_size = sizeof(zugodb_regex_v1) + 64;
    Database db;
    auto p = stub::Set(&t, nullptr);
    db.SetProviders(&p);
    CHECK(db.GetProviderState(ProviderService::REGEX) == ProviderState::ACTIVE);
    Connection conn(db);
    CHECK(One(conn, "SELECT regexp_matches('abc', 'b')") == "true");
}

TEST_CASE("Providers - UTF-8 tables of another version, too small or with a NULL member") {
    auto bad_version = stub::Utf8Table();
    bad_version.abi_version = 2;
    auto small = stub::Utf8Table();
    small.struct_size = sizeof(zugodb_utf8_v1) - 1;
    auto null_member = stub::Utf8Table();
    null_member.count = nullptr;
    struct Want {
        const zugodb_utf8_v1 *t;
        ProviderState state;
    };
    for (auto w : {Want{&bad_version, ProviderState::WRONG_VERSION},
                   Want{&small, ProviderState::TOO_SMALL},
                   Want{&null_member, ProviderState::NULL_MEMBER}}) {
        Database db;
        auto p = stub::Set(nullptr, w.t);
        db.SetProviders(&p);
        CHECK(db.GetProviderState(ProviderService::UTF8) == w.state);
        CHECK(db.GetProviders().Utf8() == nullptr);
        stub::Count().Reset();
        Connection conn(db);
        CHECK(One(conn, "SELECT length('héllo')") == "5");
        CHECK(stub::Count().utf8_counts.load() == 0);
    }
}

// ---------------------------------------------------------------------------
// Setting the providers.
// ---------------------------------------------------------------------------

TEST_CASE("Providers - the set itself: NULL, another ABI or too small is an error") {
    Database db;
    CHECK_THROWS_AS(db.SetProviders(nullptr), InvalidInputException);
    auto p = stub::Set(nullptr, nullptr);
    p.abi_version = 2;
    CHECK_THROWS_AS(db.SetProviders(&p), InvalidInputException);
    p = stub::Set(nullptr, nullptr);
    p.struct_size = sizeof(zugodb_providers) - 1;
    CHECK_THROWS_AS(db.SetProviders(&p), InvalidInputException);
    // None of those counts as the one call.
    auto t = stub::RegexTable();
    p = stub::Set(&t, nullptr);
    db.SetProviders(&p);
    CHECK(db.GetProviderState(ProviderService::REGEX) == ProviderState::ACTIVE);
}

TEST_CASE("Providers - set once, and before the first query") {
    auto t = stub::RegexTable();
    auto p = stub::Set(&t, nullptr);
    {
        Database db;
        db.SetProviders(&p);
        CHECK_THROWS_AS(db.SetProviders(&p), InvalidInputException);
    }
    {
        Database db;
        Connection conn(db);
        conn.Query("SELECT 1");
        CHECK_THROWS_AS(db.SetProviders(&p), InvalidInputException);
        CHECK(db.GetProviderState(ProviderService::REGEX) == ProviderState::ABSENT);
        // A failed query counts too.
        Database db2;
        Connection conn2(db2);
        CHECK_THROWS(conn2.Query("SELECT * FROM nope"));
        CHECK_THROWS_AS(db2.SetProviders(&p), InvalidInputException);
    }
}

TEST_CASE("Providers - the tables are copied: the host's structs may go after the call") {
    Database db;
    {
        auto t = stub::RegexTable();
        auto p = stub::Set(&t, nullptr);
        db.SetProviders(&p);
        t = zugodb_regex_v1{}; // the host's copy is gone
    }
    Connection conn(db);
    CHECK(One(conn, "SELECT regexp_matches('abc', 'b')") == "true");
}

TEST_CASE("Providers - the reserved slots are never read") {
    auto t = stub::RegexTable();
    auto p = stub::Set(&t, nullptr);
    int junk = 0;
    p.uuid = &junk;
    p.random = &junk;
    p.decimal = &junk;
    Database db;
    db.SetProviders(&p);
    CHECK(db.GetProviderState(ProviderService::UUID) == ProviderState::RESERVED);
    CHECK(db.GetProviderState(ProviderService::RANDOM) == ProviderState::RESERVED);
    CHECK(db.GetProviderState(ProviderService::DECIMAL) == ProviderState::RESERVED);
    Connection conn(db);
    CHECK(One(conn, "SELECT regexp_matches('abc', 'b')") == "true");
    // uuid() and random() stay refused, as without a provider.
    CHECK(ErrorOf(conn, "SELECT uuid()").find("libzugodb#40") != std::string::npos);
    CHECK(ErrorOf(conn, "SELECT random()").find("libzugodb#40") != std::string::npos);
}

TEST_CASE("Providers - two databases with different providers do not cross") {
    StubDb with;
    Database without;
    Connection a(with.db), b(without);
    CHECK(One(a, "SELECT regexp_matches('abc', 'b')") == "true");
    CHECK(ErrorOf(b, "SELECT regexp_matches('abc', 'b')").find("libzugodb#40") !=
          std::string::npos);
    CHECK(One(a, "SELECT 'abc' SIMILAR TO 'abc'") == "true");
    CHECK(ErrorOf(b, "SELECT 'abc' SIMILAR TO 'abc'").find("libzugodb#40") != std::string::npos);
}

// ---------------------------------------------------------------------------
// The functions, through the stub.
// ---------------------------------------------------------------------------

TEST_CASE("Providers - the regex functions through a table") {
    StubDb s;
    Connection conn(s.db);
    CHECK(One(conn, "SELECT regexp_matches('xaby', 'ab')") == "true");
    CHECK(One(conn, "SELECT regexp_matches('xaby', 'zz')") == "false");
    CHECK(One(conn, "SELECT regexp_matches('xABy', 'ab')") == "false");
    CHECK(One(conn, "SELECT regexp_matches('xABy', 'ab', 'i')") == "true");
    CHECK(One(conn, "SELECT regexp_matches('xABy', 'ab', 'ic')") == "false"); // the last wins
    CHECK(One(conn, "SELECT regexp_matches('xABy', 'ab', 'ci')") == "true");
    CHECK(One(conn, "SELECT regexp_full_match('ab', 'ab')") == "true");
    CHECK(One(conn, "SELECT regexp_full_match('xab', 'ab')") == "false");
    CHECK(One(conn, "SELECT 'ab' SIMILAR TO 'ab'") == "true");
    CHECK(One(conn, "SELECT 'xab' SIMILAR TO 'ab'") == "false");
    CHECK(One(conn, "SELECT 'xab' NOT SIMILAR TO 'ab'") == "true");
    CHECK(One(conn, "SELECT regexp_replace('a-a-a', 'a', 'b')") == "b-a-a");
    CHECK(One(conn, "SELECT regexp_replace('a-a-a', 'a', 'b', 'g')") == "b-b-b");
    CHECK(One(conn, "SELECT regexp_replace('xaby', 'a(b)', '<\\1\\0>')") == "x<bab>y");
    CHECK(One(conn, "SELECT regexp_replace('xyz', 'a', 'b')") == "xyz");
    CHECK(One(conn, "SELECT regexp_extract('xaby', 'a(b)')") == "ab");
    CHECK(One(conn, "SELECT regexp_extract('xaby', 'a(b)', 1)") == "b");
    CHECK(One(conn, "SELECT regexp_extract('xyz', 'a(b)', 1)") == "");
    CHECK(One(conn, "SELECT regexp_extract('xABy', 'a(b)', 1, 'i')") == "B");
    // NULL in, NULL out.
    CHECK(One(conn, "SELECT regexp_matches(NULL, 'a')") == "NULL");
    CHECK(One(conn, "SELECT regexp_matches('a', NULL)") == "NULL");
    CHECK(One(conn, "SELECT regexp_replace('a', 'a', NULL)") == "NULL");
    CHECK(One(conn, "SELECT regexp_extract(CAST(NULL AS VARCHAR), 'a')") == "NULL");
    // In a WHERE, over a table.
    conn.Query("CREATE TABLE t (s VARCHAR)");
    conn.Query("INSERT INTO t VALUES ('apple'), ('banana'), (NULL), ('cherry')");
    CHECK(One(conn, "SELECT count(*) FROM t WHERE regexp_matches(s, 'an')") == "1");
    CHECK(One(conn, "SELECT count(*) FROM t WHERE s SIMILAR TO 'apple'") == "1");
    auto r = conn.Query("SELECT regexp_replace(s, 'a', 'A', 'g') FROM t WHERE s IS NOT NULL ORDER BY s");
    REQUIRE(r.RowCount() == 3);
    CHECK(r.GetValue(0, 0).ToString() == "Apple");
    CHECK(r.GetValue(1, 0).ToString() == "bAnAnA");
    CHECK(r.GetValue(2, 0).ToString() == "cherry");
}

TEST_CASE("Providers - what stays refused with a regex table") {
    StubDb s;
    Connection conn(s.db);
    auto refused = [&](const std::string &sql, const std::string &part) {
        INFO(sql);
        auto e = ErrorOf(conn, sql);
        CHECK(e.find(part) != std::string::npos);
    };
    // Forms this stage leaves to later, with #40's error.
    for (auto *q : {"SELECT 'abc' ~ 'abc'", "SELECT 'abc' !~ 'abc'",
                    "SELECT regexp_split_to_array('abc', 'b')",
                    "SELECT regexp_extract_all('abc', 'b')", "SELECT string_split_regex('abc', 'b')",
                    "SELECT regexp_escape('a.b')", "SELECT regexp_like('abc', 'b')"}) {
        refused(q, "libzugodb#40");
    }
    refused("SELECT 'abc' SIMILAR TO 'a' ESCAPE '!'", "ESCAPE");
    // Options: unknown letters, 'g' outside regexp_replace, and a column.
    refused("SELECT regexp_matches('abc', 'b', 'x')", "option 'x'");
    refused("SELECT regexp_matches('abc', 'b', 's')", "option 's'");
    refused("SELECT regexp_matches('abc', 'b', 'g')", "option 'g'");
    refused("SELECT regexp_matches('abc', 'b', NULL)", "constant string");
    conn.Query("CREATE TABLE o (opt VARCHAR, g INTEGER)");
    conn.Query("INSERT INTO o VALUES ('i', 1)");
    refused("SELECT regexp_matches('abc', 'b', opt) FROM o", "constant string");
    // regexp_extract's group: a constant, within the pattern's groups.
    refused("SELECT regexp_extract('abc', 'b', g) FROM o", "constant integer");
    refused("SELECT regexp_extract('abc', 'b', 1)", "group(s)");
    refused("SELECT regexp_extract('abc', 'b', -1)", "regexp_extract");
    // Arguments that are not text, and counts.
    refused("SELECT regexp_matches(1, 'b')", "No function matches");
    refused("SELECT regexp_matches('a')", "No function matches");
    refused("SELECT regexp_replace('a', 'b')", "No function matches");
    // A pattern the provider refuses is an error at bind.
    refused("SELECT regexp_matches('abc', 'a(b')", "the pattern is refused");
}

TEST_CASE("Providers - a constant pattern is compiled once per query, match state per call") {
    StubDb s;
    Connection conn(s.db);
    conn.Query("CREATE TABLE t (s VARCHAR, p VARCHAR)");
    // 5000 rows: three chunks.
    DataChunk chunk;
    for (int base = 0; base < 5000; base += (int)VECTOR_SIZE) {
        int n = std::min<int>((int)VECTOR_SIZE, 5000 - base);
        chunk.Initialize({LogicalType::VARCHAR(), LogicalType::VARCHAR()});
        for (int j = 0; j < n; j++) {
            int v = base + j;
            chunk.SetValue(0, (idx_t)j, Value::VARCHAR(v % 3 == 0 ? "xaby" : "xyz"));
            chunk.SetValue(1, (idx_t)j, Value::VARCHAR(v < 2500 ? "ab" : "yz"));
        }
        chunk.SetCardinality((idx_t)n);
        conn.Append("t", chunk);
    }
    stub::Count().Reset();
    CHECK(One(conn, "SELECT count(*) FROM t WHERE regexp_matches(s, 'ab')") == "1667");
    CHECK(stub::Count().compiles.load() == 1);
    CHECK(stub::Count().matches_created.load() >= 3); // one per chunk
    CHECK(stub::Count().searches.load() == 5000);
    CHECK(stub::Count().patterns_live.load() == 0);
    CHECK(stub::Count().matches_live.load() == 0);

    // A pattern per row: compiled when it changes, not per row.
    stub::Count().Reset();
    CHECK(One(conn, "SELECT count(*) FROM t WHERE regexp_matches(s, p)") ==
          std::to_string(834 + 1667));
    CHECK(stub::Count().compiles.load() >= 2);
    CHECK(stub::Count().compiles.load() <= 4); // three chunks, one change
    CHECK(stub::Count().patterns_live.load() == 0);
    CHECK(stub::Count().matches_live.load() == 0);
}

TEST_CASE("Providers - every error status is a query error") {
    StubDb s;
    Connection conn(s.db);
    struct St {
        int code;
        const char *text;
    };
    const std::vector<St> statuses = {
        {ZUGODB_PROVIDER_ERR_LIMIT, "stub: match limit"},
        {ZUGODB_PROVIDER_ERR_INVALID_UTF8, "stub: invalid UTF-8"},
        {ZUGODB_PROVIDER_ERR_BAD_PATTERN, "stub: bad pattern"},
        {ZUGODB_PROVIDER_ERR_BAD_REWRITE, "stub: bad rewrite"},
        {ZUGODB_PROVIDER_ERR_BUFFER_TOO_SMALL, "stub: buffer too small"},
        {ZUGODB_PROVIDER_ERR_NOMEM, "stub: out of memory"},
        {ZUGODB_PROVIDER_ERR_ARGUMENT, "stub: bad argument"},
        {ZUGODB_PROVIDER_ERR_OTHER, "stub: other"},
        {-42, "provider status -42"}, // unknown: strerror gives NULL
        {7, "provider status 7"},     // not a status the header defines
    };
    const std::vector<std::string> members = {"compile", "group_count", "match_create", "search",
                                              "group", "replace"};
    for (auto &member : members) {
        for (auto &st : statuses) {
            // search's NOMATCH is no error; any other code is. A status the
            // engine reads at a member that returns OK/NOMATCH only must not
            // be taken as either.
            std::string pat = "!" + member + ":" + std::to_string(st.code);
            std::string sql;
            if (member == "group") {
                sql = "SELECT regexp_extract('abc', '" + pat + "')";
            } else if (member == "replace") {
                sql = "SELECT regexp_replace('abc', '" + pat + "', 'x')";
            } else {
                sql = "SELECT regexp_matches('abc', '" + pat + "')";
            }
            if (member == "replace" && st.code == ZUGODB_PROVIDER_ERR_BUFFER_TOO_SMALL) {
                continue; // the retry protocol, tested below
            }
            if (member == "group_count" && st.code > 0) {
                continue; // a positive group count is a count
            }
            INFO(sql);
            stub::Count().Reset();
            std::string e = ErrorOf(conn, sql);
            CHECK(e.find(st.text) != std::string::npos);
            if (st.code == ZUGODB_PROVIDER_ERR_LIMIT && member != "compile") {
                CHECK(e.find("hit a limit") != std::string::npos);
            }
            // Nothing is left behind by the throw.
            CHECK(stub::Count().patterns_live.load() == 0);
            CHECK(stub::Count().matches_live.load() == 0);
        }
    }
    // The same through a pattern computed per row, at execution.
    conn.Query("CREATE TABLE p (pat VARCHAR)");
    conn.Query("INSERT INTO p VALUES ('!search:-1')");
    CHECK(ErrorOf(conn, "SELECT regexp_matches('abc', pat) FROM p").find("hit a limit") !=
          std::string::npos);
    CHECK(stub::Count().matches_live.load() == 0);
}

TEST_CASE("Providers - a short replace buffer is retried once, at the size asked for") {
    StubDb s;
    Connection conn(s.db);
    CHECK(One(conn, "SELECT regexp_replace('a-a', '!grow:a', 'bbbb', 'g')") == "bbbb-bbbb");
    // A provider that asks for no more than it had is an error, not a loop.
    CHECK(ErrorOf(conn, "SELECT regexp_replace('a-a', '!shrink:a', 'b')")
              .find("stub: buffer too small") != std::string::npos);
    // A result longer than the first guess.
    std::string many(200, 'a');
    CHECK(One(conn, "SELECT length(regexp_replace('" + many + "', 'a', 'xyz', 'g'))") == "600");
}

TEST_CASE("Providers - LENGTH counts through the UTF-8 table, with the same answer") {
    Database plain;
    Connection pc(plain);
    StubDb s;
    Connection sc(s.db);
    for (auto *q : {"SELECT length('héllo')", "SELECT length('日本語')", "SELECT length('abc')",
                    "SELECT char_length('ñandú')", "SELECT length(NULL)"}) {
        INFO(q);
        stub::Count().Reset();
        CHECK(One(sc, q) == One(pc, q));
    }
    stub::Count().Reset();
    One(sc, "SELECT length('héllo')");
    CHECK(stub::Count().utf8_counts.load() == 1); // non-ASCII: through the table
    stub::Count().Reset();
    One(sc, "SELECT length('hello')");
    CHECK(stub::Count().utf8_counts.load() == 0); // ASCII: bytes
}

TEST_CASE("Providers - CountCodePoints: invalid text falls back, an error is an error") {
    auto t = stub::Utf8Table();
    const std::string valid = "h\xC3\xA9llo";            // héllo
    const std::string invalid = "h\xC3llo\xFF";          // a cut sequence, a stray byte
    CHECK(CountCodePoints(nullptr, valid.data(), valid.size()) == 5);
    stub::Count().Reset();
    CHECK(CountCodePoints(&t, valid.data(), valid.size()) == 5);
    CHECK(stub::Count().utf8_counts.load() == 1);
    stub::Count().Reset();
    CHECK(CountCodePoints(&t, invalid.data(), invalid.size()) ==
          CountCodePoints(nullptr, invalid.data(), invalid.size()));
    CHECK(stub::Count().utf8_counts.load() == 0); // the engine's count
    const std::string fails = "a\x01" "b";
    CHECK_THROWS_WITH_AS(CountCodePoints(&t, fails.data(), fails.size()),
                         doctest::Contains("stub: utf8 failure"), InvalidInputException);
    // A count past the bytes is no count of UTF-8.
    t.count = [](const char *, size_t n, size_t *out) {
        *out = n + 1;
        return ZUGODB_PROVIDER_OK;
    };
    CHECK_THROWS_AS(CountCodePoints(&t, valid.data(), valid.size()), InvalidInputException);
}

// ---------------------------------------------------------------------------
// Threads (design L29). No query calls a provider on a worker thread: the
// one parallel query path, the window's partition sort, compares keys and
// evaluates no expression. So each provider path is driven from 1, 2 and 4
// threads here directly, as a parallel operator would, with one compiled
// pattern shared and match state per thread; the linux-tsan job runs this.
// ---------------------------------------------------------------------------

namespace {

std::vector<std::string> Subjects() {
    std::vector<std::string> out;
    for (int i = 0; i < 4000; i++) {
        std::string s = "row" + std::to_string(i);
        if (i % 3 == 0) s += "-ab-";
        if (i % 5 == 0) s += "-AB-";
        if (i % 7 == 0) s += "h\xC3\xA9";
        out.push_back(s);
    }
    return out;
}

// Each subject's search, extract, replace and count, from `threads`
// threads splitting the subjects; one pattern shared by all of them.
std::vector<std::string> RunThreads(unsigned threads, const ProviderRegex &re,
                                    const zugodb_utf8_v1 *utf8,
                                    const std::vector<std::string> &subjects) {
    std::vector<std::string> out(subjects.size());
    std::vector<std::thread> ts;
    std::vector<std::string> errors(threads);
    for (unsigned t = 0; t < threads; t++) {
        ts.emplace_back([&, t] {
            try {
                ProviderRegexMatch md(re); // per thread
                for (size_t i = t; i < subjects.size(); i += threads) {
                    const auto &s = subjects[i];
                    std::string r = md.Search(s.data(), s.size(), false) ? "1" : "0";
                    size_t b = 0, e = 0;
                    if (r == "1" && md.Group(1, b, e)) r += s.substr(b, e - b);
                    r += "|" + md.Replace(s.data(), s.size(), "[\\1]", true);
                    r += "|" + std::to_string(CountCodePoints(utf8, s.data(), s.size()));
                    out[i] = r;
                }
            } catch (const std::exception &e) {
                errors[t] = e.what();
            }
        });
    }
    for (auto &th : ts) th.join();
    for (auto &e : errors) CHECK(e.empty());
    return out;
}

} // namespace

TEST_CASE("Providers - threads: one pattern shared, match state per thread, at 1, 2 and 4") {
    auto api = stub::RegexTable();
    auto utf8 = stub::Utf8Table();
    auto subjects = Subjects();
    for (uint32_t flags : {0u, (uint32_t)ZUGODB_REGEX_CASELESS}) {
        stub::Count().Reset();
        auto re = ProviderRegex::Compile(api, "regexp_test", "a(b)", flags);
        auto expected = RunThreads(1, *re, &utf8, subjects);
        // An oracle independent of threads: the subjects' own construction.
        for (size_t i = 0; i < subjects.size(); i++) {
            bool lower = i % 3 == 0, upper = i % 5 == 0;
            bool hit = lower || (flags && upper);
            CHECK(expected[i][0] == (hit ? '1' : '0'));
        }
        for (unsigned n : {2u, 4u}) {
            INFO("threads " << n << ", flags " << flags);
            CHECK(RunThreads(n, *re, &utf8, subjects) == expected);
        }
        CHECK(stub::Count().compiles.load() == 1);
        re.reset();
        CHECK(stub::Count().patterns_live.load() == 0);
        CHECK(stub::Count().matches_live.load() == 0);
    }
}

TEST_CASE("Providers - threads: the parallel window sort over a regex key") {
    // The window operator sorts its partitions on workers from 200,000 rows
    // and two partitions at cap 2 (test_threads.cpp). Its partition key here
    // is a regex result computed below it, on the query's thread; the rows
    // must be the same at caps 1, 2 and 4.
    struct CapGuard {
        unsigned int old = ThreadCap();
        ~CapGuard() { SetThreadCap(old); }
    } guard;
    StubDb s;
    Connection conn(s.db);
    conn.Query("CREATE TABLE w (s VARCHAR, v BIGINT)");
    const int64_t rows = 200000;
    for (int64_t base = 0; base < rows; base += (int64_t)VECTOR_SIZE) {
        int64_t n = std::min<int64_t>((int64_t)VECTOR_SIZE, rows - base);
        DataChunk chunk;
        chunk.Initialize({LogicalType::VARCHAR(), LogicalType::BIGINT()});
        for (int64_t j = 0; j < n; j++) {
            int64_t v = base + j;
            chunk.SetValue(0, (idx_t)j, Value::VARCHAR(v % 2 ? "xaby" : "xyz"));
            chunk.SetValue(1, (idx_t)j, Value::BIGINT(v));
        }
        chunk.SetCardinality((idx_t)n);
        conn.Append("w", chunk);
    }
    const char *sql =
        "SELECT k, v, row_number() OVER (PARTITION BY k ORDER BY v) "
        "FROM (SELECT regexp_matches(s, 'ab') AS k, v FROM w) AS q";
    // An oracle independent of the engine: v is in partition v % 2, number
    // v / 2 + 1.
    for (unsigned n : {1u, 2u, 4u}) {
        SetThreadCap(n);
        INFO("cap " << n);
        stub::Count().Reset();
        auto r = conn.Query(sql);
        REQUIRE(r.RowCount() == (idx_t)rows);
        idx_t bad = 0;
        for (idx_t i = 0; i < r.RowCount(); i++) {
            bool k = r.GetValue(i, 0).GetValue<bool>();
            int64_t v = r.GetValue(i, 1).GetValue<int64_t>();
            int64_t rn = r.GetValue(i, 2).GetValue<int64_t>();
            if (k != (v % 2 == 1) || rn != v / 2 + 1) bad++;
        }
        CHECK(bad == 0);
        CHECK(stub::Count().compiles.load() == 1);
        CHECK(stub::Count().matches_live.load() == 0);
    }
    // And at cap 2 the sort does run on a worker.
    SetThreadCap(2);
    if (HWThreads() >= 2) {
        ArmWorkerFault(WorkerSite::FORK_JOIN);
        try {
            conn.Query(sql);
        } catch (const InternalException &) {
        }
        auto hits = WorkerFaultHits();
        DisarmWorkerFault();
        CHECK(hits > 0);
    }
}
