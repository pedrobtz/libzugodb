// Running one case and classifying it (design §5, L15, L16).
//
// The comparison of a query's result is DuckDB's (result_helper.cpp,
// CheckQueryResult): the result is formatted value by value, sorted as
// the sort mode says, hashed when the expected value is a hash or the
// query has a label, and otherwise compared with the recorded values,
// which may be one value per line or one row per line with tabs. A
// column count other than the type string's is a wrong result, as there.

#include "slt.hpp"

#include "slothdb/main/connection.hpp"
#include "slothdb/main/database.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace zugodb::slt {

const char *OutcomeName(Outcome outcome) {
    switch (outcome) {
    case Outcome::Passed: return "passed";
    case Outcome::Refused: return "refused";
    case Outcome::Wrong: return "wrong";
    case Outcome::Accepted: return "accepted";
    case Outcome::Unreliable: return "unreliable";
    case Outcome::Crashed: return "crashed";
    case Outcome::Hung: return "hung";
    case Outcome::NotRun: return "not_run";
    }
    return "?";
}

bool ParseOutcome(const std::string &name, Outcome &outcome) {
    for (auto o : {Outcome::Passed, Outcome::Refused, Outcome::Wrong, Outcome::Accepted, Outcome::Unreliable,
                   Outcome::Crashed, Outcome::Hung, Outcome::NotRun})
        if (name == OutcomeName(o)) {
            outcome = o;
            return true;
        }
    return false;
}

namespace {

std::string OneLine(const std::string &s, size_t max = 200) {
    std::string out;
    for (char ch : s) {
        if (out.size() >= max) {
            out += "...";
            break;
        }
        if (ch == '\n') out += "\\n";
        else if (ch == '\t') out += "\\t";
        else if (ch == '\r') continue;
        else out += ch;
    }
    return out;
}

bool IsHash(const std::string &s) {
    size_t pos = 0;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') pos++;
    if (pos == 0) return false;
    const std::string mid = " values hashing to ";
    if (s.size() != pos + mid.size() + 32) return false;
    if (s.compare(pos, mid.size(), mid) != 0) return false;
    for (size_t i = pos + mid.size(); i < s.size(); i++) {
        char ch = s[i];
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z'))) return false;
    }
    return true;
}

void SortResult(SortMode mode, std::vector<std::string> &values, size_t ncols) {
    if (mode == SortMode::None) return;
    if (mode == SortMode::Value) {
        std::sort(values.begin(), values.end());
        return;
    }
    if (ncols == 0 || values.size() % ncols != 0) return;
    size_t nrows = values.size() / ncols;
    std::vector<std::vector<std::string>> rows(nrows);
    for (size_t r = 0; r < nrows; r++)
        for (size_t c = 0; c < ncols; c++) rows[r].push_back(std::move(values[r * ncols + c]));
    std::sort(rows.begin(), rows.end());
    for (size_t r = 0; r < nrows; r++)
        for (size_t c = 0; c < ncols; c++) values[r * ncols + c] = std::move(rows[r][c]);
}

// As DuckDB's StringUtil::Split, which its runner uses: an empty piece is
// dropped (DuckDB prints an empty value as "(empty)", so none is a value),
// and a line of none is itself. A recorded row with two tabs in a row
// (aggregate/qualify/test_qualify.test) is DuckDB's row of three.
std::vector<std::string> SplitTabs(const std::string &s) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : s) {
        if (ch == '\t') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += ch;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    if (out.empty()) out.push_back(s);
    return out;
}

ColumnKind KindOf(LogicalTypeId id) {
    switch (id) {
    case LogicalTypeId::BOOLEAN:
        return ColumnKind::Bool;
    case LogicalTypeId::TINYINT:
    case LogicalTypeId::SMALLINT:
    case LogicalTypeId::INTEGER:
    case LogicalTypeId::BIGINT:
    case LogicalTypeId::HUGEINT:
    case LogicalTypeId::UTINYINT:
    case LogicalTypeId::USMALLINT:
    case LogicalTypeId::UINTEGER:
    case LogicalTypeId::UBIGINT:
        return ColumnKind::Integer;
    case LogicalTypeId::FLOAT:
    case LogicalTypeId::DOUBLE:
        return ColumnKind::Float;
    default:
        return ColumnKind::Other;
    }
}

[[noreturn]] void Hang() {
    for (;;) std::this_thread::sleep_for(std::chrono::seconds(1));
}

} // namespace

Runner::Runner(bool test_hooks) : test_hooks_(test_hooks) { Restart(); }

Runner::~Runner() = default;

void Runner::Restart() {
    con_.reset();
    db_ = std::make_unique<Database>();
    con_ = std::make_unique<Connection>(*db_);
    tainted_by_ = 0;
}

CaseResult Runner::Run(const Case &c) {
    switch (c.kind) {
    case Case::Kind::Restart:
        Restart();
        return {Outcome::Passed, ""};
    case Case::Kind::Statement:
        return RunStatement(c);
    case Case::Kind::Query:
        return RunQuery(c);
    }
    return {};
}

CaseResult Runner::RunStatement(const Case &c) {
    if (test_hooks_ && c.sql == kHookCrash) std::abort();
    if (test_hooks_ && c.sql == kHookHang) Hang();
    std::string error;
    bool failed = false;
    try {
        con_->Query(c.sql);
    } catch (const std::exception &e) {
        failed = true;
        error = e.what();
    } catch (...) {
        failed = true;
        error = "unknown exception";
    }
    CaseResult r;
    switch (c.expect) {
    case Expect::Ok:
        if (!failed) {
            r.outcome = Outcome::Passed;
        } else {
            r.outcome = Outcome::Refused;
            r.detail = OneLine(error);
            if (!tainted_by_) tainted_by_ = c.line;
        }
        break;
    case Expect::Error:
        if (failed) {
            r.outcome = Outcome::Passed;
        } else {
            r.outcome = Outcome::Accepted;
            if (tainted_by_) {
                r.outcome = Outcome::Unreliable;
                r.detail = "accepted, after line " + std::to_string(tainted_by_);
            }
            if (!tainted_by_) tainted_by_ = c.line;
        }
        break;
    case Expect::Maybe:
        r.outcome = Outcome::Passed;
        break;
    }
    return r;
}

CaseResult Runner::RunQuery(const Case &c) {
    CaseResult r;
    QueryResult result;
    try {
        result = con_->Query(c.sql);
    } catch (const std::exception &e) {
        r.outcome = Outcome::Refused;
        r.detail = OneLine(e.what());
        return r;
    } catch (...) {
        r.outcome = Outcome::Refused;
        r.detail = "unknown exception";
        return r;
    }
    std::string detail;
    bool ok;
    try {
        ok = CompareQuery(c, result, detail);
    } catch (const std::exception &e) {
        ok = false;
        detail = std::string("reading the result failed: ") + e.what();
    }
    if (ok) {
        r.outcome = Outcome::Passed;
    } else if (tainted_by_) {
        r.outcome = Outcome::Unreliable;
        r.detail = OneLine("wrong, after line " + std::to_string(tainted_by_) + ": " + detail, 300);
    } else {
        r.outcome = Outcome::Wrong;
        r.detail = OneLine(detail, 300);
    }
    return r;
}

// Returns whether the result matches, and if not, why not in `detail`.
bool Runner::CompareQuery(const Case &c, const QueryResult &result, std::string &detail) {
    size_t ncols = result.ColumnCount();
    size_t nrows = result.RowCount();
    size_t total = ncols * nrows;
    std::vector<std::string> got;
    got.reserve(total);
    for (size_t row = 0; row < nrows; row++)
        for (size_t col = 0; col < ncols; col++) got.push_back(FormatValue(result.GetValue(row, col)));
    SortResult(c.sort, got, ncols);

    std::string joined;
    for (auto &v : got) joined += v + "\n";
    std::string md5 = Md5Hex(joined);
    std::string hash_value = std::to_string(total) + " values hashing to " + md5;
    auto fail = [&](const std::string &why) {
        detail = why;
        return false;
    };

    bool result_is_hash = c.values.size() == 1 && IsHash(c.values[0]);
    bool compare_hash =
        !c.label.empty() || (c.hash_threshold > 0 && total > static_cast<size_t>(c.hash_threshold)) ||
        result_is_hash;
    if (compare_hash) {
        if (!c.label.empty()) {
            auto it = std::find_if(labels_.begin(), labels_.end(), [&](auto &p) { return p.first == c.label; });
            if (it == labels_.end()) labels_.emplace_back(c.label, hash_value);
            else if (it->second != hash_value) return fail("label " + c.label + ": " + hash_value);
        }
        if (result_is_hash && c.values[0] != hash_value) return fail("got " + hash_value);
        return true;
    }

    size_t expected_cols = c.types.size();
    if (expected_cols != ncols)
        return fail(std::to_string(ncols) + " columns, expected " + std::to_string(expected_cols));
    if (ncols == 0) return fail("no columns");

    std::vector<std::string> expected = c.values;
    bool row_wise = ncols > 1 && expected.size() == nrows;
    if (!row_wise) {
        bool all_tabs = !expected.empty();
        for (auto &v : expected)
            if (v.find('\t') == std::string::npos) {
                all_tabs = false;
                break;
            }
        row_wise = all_tabs;
    }
    size_t expected_rows;
    if (row_wise) {
        expected_rows = expected.size();
    } else {
        if (expected.size() % ncols != 0)
            return fail(std::to_string(expected.size()) + " values do not divide into " + std::to_string(ncols) +
                        " columns");
        expected_rows = expected.size() / ncols;
    }
    if (expected_rows != nrows)
        return fail(std::to_string(nrows) + " rows, expected " + std::to_string(expected_rows));
    if (row_wise) {
        std::vector<std::string> split;
        for (auto &line : expected) {
            auto cells = SplitTabs(line);
            if (cells.size() != ncols)
                return fail("expected row has " + std::to_string(cells.size()) + " values, not " +
                            std::to_string(ncols));
            for (auto &cell : cells) split.push_back(std::move(cell));
        }
        expected = std::move(split);
    }
    for (size_t i = 0; i < total; i++) {
        size_t col = i % ncols;
        char declared = col < c.types.size() ? c.types[col] : 0;
        if (!ValueMatches(got[i], expected[i], (col < result.column_types.size() ? KindOf(result.column_types[col].id()) : ColumnKind::Other), declared))
            return fail("row " + std::to_string(i / ncols + 1) + " column " + std::to_string(col + 1) + ": got " +
                        got[i] + ", expected " + expected[i]);
    }
    return true;
}

void RunScript(const Script &script, bool test_hooks, const std::function<void(size_t)> &on_begin,
               const std::function<void(size_t, const CaseResult &)> &on_end) {
    Runner runner(test_hooks);
    size_t index = 0;
    for (auto &c : script.cases) {
        if (c.kind == Case::Kind::Restart) {
            runner.Run(c);
            continue;
        }
        if (on_begin) on_begin(index);
        CaseResult r = runner.Run(c);
        if (on_end) on_end(index, r);
        index++;
    }
}

} // namespace zugodb::slt
