#pragma once

// The conformance runner (libzugodb Stage 6, design §5 and L14 to L17).
//
// It reads DuckDB's sqllogictest files as far as the pinned subset needs
// them, runs each case through Connection::Query, and classifies it:
// passed, refused, wrong, accepted, crashed, hung, or unreliable (a wrong
// or accepted case after a setup statement of its file failed). The
// comparison follows DuckDB's own runner at the pinned tag
// (test/sqlite/result_helper.cpp), with the stricter numeric rule of L15.
//
// A file is parsed and expanded first (loops, foreach, skipif/onlyif,
// mode skip, halt) into a flat list of cases, without the engine, so the
// driver and the child process that runs the file agree on what case k
// is. A file that needs something the runner does not do is left out
// with a reason, before anything runs.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace zugodb {
class Value;
struct QueryResult;
class Database;
class Connection;
} // namespace zugodb

namespace zugodb::slt {

enum class Outcome { Passed, Refused, Wrong, Accepted, Unreliable, Crashed, Hung, NotRun };
const char *OutcomeName(Outcome outcome);
bool ParseOutcome(const std::string &name, Outcome &outcome);

enum class Expect { Ok, Error, Maybe };
enum class SortMode { None, Row, Value };

struct Case {
    enum class Kind { Statement, Query, Restart };
    Kind kind = Kind::Statement;
    int line = 0;  // 1-based line of the `statement` or `query` line
    std::string sql;
    // statement
    Expect expect = Expect::Ok;
    std::string expected_error;
    // query
    std::string types;  // the type string, e.g. "IIT"; its length is the column count
    SortMode sort = SortMode::None;
    std::string label;
    std::vector<std::string> values;
    int hash_threshold = 0;
};

struct Script {
    std::string name;
    std::string left_out;  // empty when the file runs; otherwise why not
    std::vector<Case> cases;  // statements and queries, and restarts between them
    size_t static_cases = 0;  // `statement` and `query` records in the file, unexpanded
    size_t CaseCount() const;  // statements and queries after expansion
};

// Parses and expands a file. `test_hooks` lets the statements of the
// runner's own fixtures through (see TestHookStatement).
Script ParseScript(const std::string &text, const std::string &name, bool test_hooks = false);
Script ParseFile(const std::string &path, const std::string &name, bool test_hooks = false);

// DuckDB's text for a value, as its sqllogictest runner prints it:
// NULL, booleans as 1 and 0, the empty string as (empty), doubles as
// DuckDB's fmt formats them (shortest round trip, ".0" on whole numbers,
// an exponent outside [1e-4, 1e16)), dates with "(BC)", and so on.
std::string FormatValue(const Value &value);
std::string FormatDouble(double value);
std::string FormatFloat(float value);

// Whether one formatted value matches the recorded one (L15). `kind` is
// that of the engine's column, `declared` the type character the test
// gives the column ('I', 'T', 'R', or 0 if none).
enum class ColumnKind { Other, Bool, Integer, Float };
bool ValueMatches(const std::string &actual, const std::string &expected, ColumnKind kind, char declared);

std::string Md5Hex(const std::string &data);

struct CaseResult {
    Outcome outcome = Outcome::NotRun;
    std::string detail;  // why a case is wrong, refused or accepted; one line
};

// Runs the cases of one file against a fresh in-memory database.
class Runner {
public:
    explicit Runner(bool test_hooks = false);
    ~Runner();
    // A restart gives a new database and resets nothing else.
    CaseResult Run(const Case &c);

private:
    void Restart();
    CaseResult RunStatement(const Case &c);
    CaseResult RunQuery(const Case &c);
    bool CompareQuery(const Case &c, const QueryResult &result, std::string &detail);

    bool test_hooks_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<Connection> con_;
    // A setup statement failed, or one expected to fail did not: later
    // wrong or accepted cases of the file are unreliable (L16).
    int tainted_by_ = 0;
    std::vector<std::pair<std::string, std::string>> labels_;
};

// Runs every case of a script in this process, reporting each as it ends.
void RunScript(const Script &script, bool test_hooks, const std::function<void(size_t index)> &on_begin,
               const std::function<void(size_t index, const CaseResult &)> &on_end);

// ---------------------------------------------------------------------------
// The driver (POSIX): one child process per file, a time limit per case.
// ---------------------------------------------------------------------------

struct FileRun {
    std::string file;
    std::string left_out;
    std::vector<CaseResult> results;  // one per case of the expanded script
    std::vector<int> lines;           // the line of each case
    std::vector<std::string> sqls;    // the SQL of each case
    std::string crash_note;           // how the child ended, when it did not finish
};

struct DriverOptions {
    std::string binary;      // the zugodb_slt executable, run as `binary --child file cap`
    unsigned thread_cap = 1;
    unsigned jobs = 1;
    double case_timeout = 30;  // seconds without progress before a case is hung
    uint64_t max_resident_mib = 2048;  // a child past this much memory is killed, its case hung
    bool test_hooks = false;
    bool verbose = false;      // keep the children's stderr
};

// `files` are pairs of (path on disk, name in the outputs).
std::vector<FileRun> RunFiles(const std::vector<std::pair<std::string, std::string>> &files,
                              const DriverOptions &options);

// The child's side: runs one file, writing its protocol to fd 3.
int ChildMain(const std::string &path, unsigned thread_cap, bool test_hooks);

// Outputs, deterministic: counts per file, totals, failures, and the
// cases whose result differs between two thread caps.
std::string CountsTsv(const std::vector<FileRun> &runs);
std::string TotalsTsv(const std::vector<FileRun> &runs);
std::string FailuresTsv(const std::vector<FileRun> &runs);
std::string DetailsTsv(const std::vector<FileRun> &runs);
std::string ThreadDiffTsv(const std::vector<FileRun> &a, unsigned cap_a,
                          const std::vector<FileRun> &b, unsigned cap_b);

// Test hooks for the runner's own fixtures, never in the corpus and only
// with test_hooks on: a statement whose SQL is exactly one of these makes
// the runner abort() or loop for ever instead of calling the engine.
inline constexpr const char *kHookCrash = "-- zugodb_slt: crash";
inline constexpr const char *kHookHang = "-- zugodb_slt: hang";

} // namespace zugodb::slt
