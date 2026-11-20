// NaN, the infinities, -0.0 and NULL as DuckDB has them (libzugodb #42,
// design L30). The oracle is test/fixtures/nan/duckdb_cases.tsv, DuckDB
// v2.0.0's answers to some 1,100 queries (tools/make_nan_cases.py, which
// documents the format), replayed here at thread caps 1 and 2 and over
// tables saved to a database file and loaded. Each case must give DuckDB's
// rows, or raise an error where DuckDB does; one the engine refuses where
// DuckDB answers is listed with its reason in test/fixtures/nan/refused.tsv,
// and a listed case that the engine answers fails as well, so the list
// cannot go stale.
#include "doctest.h"

#include "zugodb/common/parallel.hpp"
#include "zugodb/common/types/float_order.hpp"
#include "zugodb/main/connection.hpp"
#include "zugodb/main/database.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace zugodb;

namespace {

const std::string NAN_DIR = std::string(ZUGODB_SOURCE_DIR) + "/test/fixtures/nan/";

struct Case {
    int id = 0;
    std::string mode;
    std::string sql;
    std::string expected;
};

std::vector<std::string> Split(const std::string &s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

std::vector<Case> LoadCases() {
    std::ifstream in(NAN_DIR + "duckdb_cases.tsv");
    REQUIRE(in.good());
    std::vector<Case> cases;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto f = Split(line, '\t');
        REQUIRE(f.size() == 4);
        cases.push_back({std::stoi(f[0]), f[1], f[2], f[3]});
    }
    return cases;
}

// The SQL of each listed case, and why the engine refuses it.
std::map<std::string, std::string> LoadRefused() {
    std::ifstream in(NAN_DIR + "refused.tsv");
    REQUIRE(in.good());
    std::map<std::string, std::string> out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto tab = line.find('\t');
        REQUIRE(tab != std::string::npos);
        out[line.substr(0, tab)] = line.substr(tab + 1);
    }
    return out;
}

std::string Unescape(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            out += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

// One value of the engine's against one of DuckDB's, by the encoding of
// tools/make_nan_cases.py. `loose`: -0.0 matches 0.0. A finite double
// matches within 4 units in the last place (the C library's).
bool Matches(const Value &v, const std::string &want, bool loose) {
    if (want == "N") return v.IsNull();
    if (v.IsNull()) return false;
    auto id = v.type().id();
    char kind = want[0];
    std::string body = want.substr(1);
    switch (kind) {
    case 'b':
        return id == LogicalTypeId::BOOLEAN && v.GetValue<bool>() == (body == "1");
    case 'i':
        switch (id) {
        case LogicalTypeId::TINYINT: case LogicalTypeId::SMALLINT: case LogicalTypeId::INTEGER:
        case LogicalTypeId::BIGINT: case LogicalTypeId::UTINYINT: case LogicalTypeId::USMALLINT:
        case LogicalTypeId::UINTEGER:
            return std::to_string(v.GetValue<int64_t>()) == body;
        case LogicalTypeId::UBIGINT:
            return std::to_string(v.GetValue<uint64_t>()) == body;
        case LogicalTypeId::HUGEINT:
            return v.ToString() == body;
        default:
            return false;
        }
    case 'f': {
        double got;
        if (id == LogicalTypeId::DOUBLE) got = v.GetValue<double>();
        else if (id == LogicalTypeId::FLOAT) got = static_cast<double>(v.GetValue<float>());
        else return false;
        if (body == "nan") return IsNaN(got);
        if (IsNaN(got)) return false;
        double x = std::strtod(body.c_str(), nullptr);
        if (!loose && SignBit(got) != SignBit(x)) return false;
        if (got == x) return true;
        // A finite value within 4 units in the last place: the C library's
        // sin, exp, cbrt, ... differ by one between glibc, Apple's libm and
        // MinGW's, as DuckDB's own results do; the oracle was made on one.
        if (std::isinf(got) || std::isinf(x)) return false;
        double lo = x, hi = x;
        for (int k = 0; k < 4; k++) {
            lo = std::nextafter(lo, -std::numeric_limits<double>::infinity());
            hi = std::nextafter(hi, std::numeric_limits<double>::infinity());
        }
        return got >= lo && got <= hi;
    }
    case 's':
        return (id == LogicalTypeId::VARCHAR || id == LogicalTypeId::BLOB) &&
               v.GetValue<std::string>() == Unescape(body);
    default:
        return false;
    }
}

std::string Show(const Value &v) {
    if (v.IsNull()) return "NULL";
    return v.ToString() + "::" + v.type().ToString();
}

// "" when `r` is DuckDB's result, else what differs.
std::string Compare(const QueryResult &r, const Case &c) {
    bool ordered = c.mode[0] == 'o';
    bool loose = c.mode.find('L') != std::string::npos;
    std::vector<std::vector<std::string>> want;
    if (!c.expected.empty()) {
        for (auto &row : Split(c.expected, '|')) want.push_back(Split(row, ','));
    }
    std::vector<std::vector<Value>> got;
    for (idx_t i = 0; i < r.RowCount(); i++) {
        std::vector<Value> row;
        for (idx_t j = 0; j < r.ColumnCount(); j++) row.push_back(r.GetValue(i, j));
        got.push_back(std::move(row));
    }
    auto show_got = [&]() {
        std::string s;
        for (auto &row : got) {
            s += s.empty() ? "" : " | ";
            for (size_t j = 0; j < row.size(); j++) s += (j ? ", " : "") + Show(row[j]);
        }
        return s;
    };
    if (got.size() != want.size()) {
        return std::to_string(got.size()) + " rows, DuckDB " + std::to_string(want.size()) +
               ": " + show_got();
    }
    auto row_matches = [&](const std::vector<Value> &g, const std::vector<std::string> &w) {
        if (g.size() != w.size()) return false;
        for (size_t j = 0; j < g.size(); j++) {
            if (!Matches(g[j], w[j], loose)) return false;
        }
        return true;
    };
    if (ordered) {
        for (size_t i = 0; i < got.size(); i++) {
            if (!row_matches(got[i], want[i])) return "row " + std::to_string(i + 1) + ": " + show_got();
        }
        return "";
    }
    // A multiset: each of DuckDB's rows matched by one of the engine's.
    std::vector<bool> used(got.size(), false);
    for (auto &w : want) {
        bool found = false;
        for (size_t i = 0; i < got.size() && !found; i++) {
            if (!used[i] && row_matches(got[i], w)) used[i] = found = true;
        }
        if (!found) return "no row matches DuckDB's (" + [&] {
            std::string s;
            for (auto &x : w) s += (s.empty() ? "" : ", ") + x;
            return s;
        }() + "): " + show_got();
    }
    return "";
}

struct Outcome {
    std::vector<std::string> failures;
    std::set<std::string> refused;
};

// Runs every case on `conn`; setups only when `run_setup`.
Outcome RunCases(Connection &conn, const std::vector<Case> &cases, bool run_setup,
                 const std::map<std::string, std::string> &listed) {
    Outcome out;
    for (auto &c : cases) {
        if (c.mode == "setup") {
            if (!run_setup) continue;
            try {
                conn.Query(c.sql);
            } catch (std::exception &e) {
                out.failures.push_back("setup " + std::to_string(c.id) + " failed: " + e.what());
            }
            continue;
        }
        std::string id = "#" + std::to_string(c.id) + " " + c.sql + ": ";
        try {
            auto r = conn.Query(c.sql);
            if (c.expected == "error") {
                out.failures.push_back(id + "accepted, DuckDB raises an error");
                continue;
            }
            auto diff = Compare(r, c);
            if (!diff.empty()) out.failures.push_back(id + "wrong: " + diff);
            else if (listed.count(c.sql)) out.failures.push_back(id + "listed as refused, now answered");
        } catch (std::exception &e) {
            if (c.expected == "error") continue;
            out.refused.insert(c.sql);
            if (!listed.count(c.sql)) out.failures.push_back(id + "refused: " + e.what());
        }
    }
    return out;
}

void Report(const Outcome &o, const std::map<std::string, std::string> &listed) {
    for (auto &f : o.failures) MESSAGE(f);
    CHECK(o.failures.empty());
    CHECK(o.refused.size() == listed.size());
}

} // namespace

TEST_CASE("NaN as DuckDB has it: the oracle's cases at thread caps 1 and 2 (#42)") {
    auto cases = LoadCases();
    auto listed = LoadRefused();
    REQUIRE(cases.size() > 1000);
    auto saved = ThreadCap();
    for (unsigned cap : {1u, 2u}) {
        CAPTURE(cap);
        SetThreadCap(cap);
        Database db;
        Connection conn(db);
        Report(RunCases(conn, cases, true, listed), listed);
    }
    SetThreadCap(saved);
}

TEST_CASE("NaN as DuckDB has it: the oracle's cases over a database file loaded (#42)") {
    auto cases = LoadCases();
    auto listed = LoadRefused();
    const char *path = "test_nan_cases.db";
    std::remove(path);
    {
        Database db(path);
        Connection conn(db);
        for (auto &c : cases) {
            if (c.mode == "setup") conn.Query(c.sql);
        }
    } // saved
    {
        Database db(path);
        Connection conn(db);
        // The NaNs are loaded with their bits: the sign of each survives.
        auto r = conn.Query("SELECT CAST(d AS VARCHAR), CAST(f AS VARCHAR) FROM t WHERE id IN (1, 2, 6) ORDER BY id");
        REQUIRE(r.RowCount() == 3);
        CHECK(r.GetValue(0, 0).ToString() == "nan");
        CHECK(r.GetValue(1, 0).ToString() == "-nan");
        CHECK(r.GetValue(1, 1).ToString() == "-nan");
        CHECK(r.GetValue(2, 0).ToString() == "-0.0");
        Report(RunCases(conn, cases, false, listed), listed);
    }
    std::remove(path);
}

TEST_CASE("NaN is held by append, by a database file, and shown as DuckDB shows it (#42)") {
    const char *path = "test_nan_bits.db";
    std::remove(path);
    uint64_t payload_bits = 0x7ff80000000007a2ULL;  // R's NA_real_: a NaN, payload 1954
    double payload;
    std::memcpy(&payload, &payload_bits, sizeof payload);
    {
        Database db(path);
        Connection conn(db);
        conn.Query("CREATE TABLE t (i INTEGER, d DOUBLE, f FLOAT)");
        DataChunk chunk;
        chunk.Initialize({LogicalType::INTEGER(), LogicalType::DOUBLE(), LogicalType(LogicalTypeId::FLOAT)});
        chunk.SetValue(0, 0, Value::INTEGER(1));
        chunk.SetValue(1, 0, Value::DOUBLE(std::numeric_limits<double>::quiet_NaN()));
        chunk.SetValue(2, 0, Value::FLOAT(-std::numeric_limits<float>::quiet_NaN()));
        chunk.SetValue(0, 1, Value::INTEGER(2));
        chunk.SetValue(1, 1, Value::DOUBLE(payload));
        chunk.SetValue(2, 1, Value::FLOAT(-0.0f));
        chunk.SetCardinality(2);
        conn.Append("t", chunk);
        auto r = conn.Query("SELECT count(*) FROM t WHERE d = 'nan'::DOUBLE");
        CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 2);
    }
    {
        Database db(path);
        Connection conn(db);
        auto r = conn.Query("SELECT d, f FROM t ORDER BY i");
        REQUIRE(r.RowCount() == 2);
        double d0 = r.GetValue(0, 0).GetValue<double>();
        double d1 = r.GetValue(1, 0).GetValue<double>();
        uint64_t b1;
        std::memcpy(&b1, &d1, sizeof b1);
        CHECK(IsNaN(d0));
        CHECK(b1 == payload_bits);  // the payload is kept, bit for bit
        CHECK(IsNaN(r.GetValue(0, 1).GetValue<float>()));
        CHECK(SignBit(r.GetValue(0, 1).GetValue<float>()));
        CHECK(SignBit(r.GetValue(1, 1).GetValue<float>()));
        auto g = conn.Query("SELECT count(*) FROM t GROUP BY d");
        CHECK(g.RowCount() == 1);  // every NaN is one group
        auto t = conn.Query("SELECT CAST(f AS VARCHAR) FROM t ORDER BY i");
        CHECK(t.GetValue(0, 0).ToString() == "-nan");
        CHECK(t.GetValue(1, 0).ToString() == "-0.0");
    }
    std::remove(path);
}
