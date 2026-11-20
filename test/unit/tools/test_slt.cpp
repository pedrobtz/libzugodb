// The conformance runner itself (libzugodb Stage 6, #10; design §5, L14
// to L17), through its library in tools/slt/.
//
// - test/fixtures/slt/classes.test holds one case of each class, each
//   with its class in the comment above it; the runner must classify
//   every case as written.
// - The value formatter against DuckDB's recorded text, each value taken
//   from a file of the corpus at the pinned tag (test/conformance/).
// - A file that crashes and one that loops, through the driver and the
//   zugodb_slt program: counted as crashed and hung, and the run goes on.
// - The fixtures at thread caps 1 and 2: the same classes.

#include "doctest.h"

#include "slt.hpp"

#include "slothdb/common/types/value.hpp"

#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace zugodb;
using namespace zugodb::slt;

namespace {

const std::string FIXTURES = std::string(ZUGODB_SOURCE_DIR) + "/test/fixtures/slt/";

std::string ReadText(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

// The class written in the comment just above each `statement` or `query`
// line, in file order.
std::vector<std::pair<int, std::string>> WrittenClasses(const std::string &text) {
    std::vector<std::pair<int, std::string>> out;
    std::istringstream in(text);
    std::string line, last_comment;
    int n = 0;
    while (std::getline(in, line)) {
        n++;
        if (line.rfind("# ", 0) == 0) {
            last_comment = line.substr(2);
        } else if (line.rfind("statement ", 0) == 0 || line.rfind("query ", 0) == 0) {
            std::string word = last_comment.substr(0, last_comment.find_first_of(" :"));
            out.emplace_back(n, word);
            last_comment.clear();
        } else if (line.empty()) {
            // a comment applies to the record that follows it directly
        }
    }
    return out;
}

std::vector<CaseResult> RunInProcess(const Script &script) {
    std::vector<CaseResult> results(script.CaseCount());
    RunScript(script, false, nullptr, [&](size_t k, const CaseResult &r) { results[k] = r; });
    return results;
}

} // namespace

TEST_CASE("slt: every case of the fixture gets the class written above it") {
    std::string text = ReadText(FIXTURES + "classes.test");
    Script script = ParseScript(text, "classes.test");
    REQUIRE(script.left_out.empty());
    auto written = WrittenClasses(text);
    REQUIRE(written.size() == script.CaseCount());
    auto results = RunInProcess(script);
    std::map<std::string, int> seen;
    size_t k = 0;
    for (auto &c : script.cases) {
        if (c.kind == Case::Kind::Restart) continue;
        CAPTURE(c.line);
        CAPTURE(c.sql);
        CAPTURE(results[k].detail);
        REQUIRE(c.line == written[k].first);
        CHECK(std::string(OutcomeName(results[k].outcome)) == written[k].second);
        seen[written[k].second]++;
        k++;
    }
    // one of each class a file can give without crashing or hanging, and
    // the three planted wrong answers at least (value, type, order)
    CHECK(seen["passed"] > 0);
    CHECK(seen["refused"] > 0);
    CHECK(seen["accepted"] > 0);
    CHECK(seen["unreliable"] > 0);
    CHECK(seen["wrong"] >= 3);
}

TEST_CASE("slt: a planted wrong value, type and row order are wrong, and pass when right") {
    auto run = [](const std::string &text) {
        Script s = ParseScript(text, "inline.test");
        REQUIRE(s.left_out.empty());
        auto r = RunInProcess(s);
        REQUIRE(r.size() == 1);
        return r[0].outcome;
    };
    // value
    CHECK(run("query I\nSELECT 41 + 1\n----\n42\n") == Outcome::Passed);
    CHECK(run("query I\nSELECT 41 + 1\n----\n43\n") == Outcome::Wrong);
    // type: DuckDB's text for a DOUBLE against an integer, or against text
    CHECK(run("query R\nSELECT CAST(1 AS DOUBLE)\n----\n1.0\n") == Outcome::Passed);
    CHECK(run("query R\nSELECT 1\n----\n1.0\n") == Outcome::Wrong);
    CHECK(run("query T\nSELECT '1'\n----\n1.0\n") == Outcome::Wrong);
    // order: nosort compares row for row, rowsort sorts first
    CHECK(run("query I nosort\nSELECT * FROM (VALUES (2), (1)) v(x) ORDER BY x\n----\n2\n1\n") ==
          Outcome::Wrong);
    CHECK(run("query I rowsort\nSELECT * FROM (VALUES (2), (1)) v(x) ORDER BY x\n----\n2\n1\n") ==
          Outcome::Wrong);  // rowsort sorts the result, not the recorded values
    CHECK(run("query I rowsort\nSELECT * FROM (VALUES (2), (1)) v(x) ORDER BY x DESC\n----\n1\n2\n") ==
          Outcome::Passed);
}

TEST_CASE("slt: values are formatted as DuckDB's runner records them") {
    // test/sql/aggregate/aggregates/test_median.test:12 "NULL\t1"
    CHECK(FormatValue(Value()) == "NULL");
    // test/sql/function/string/overlay.test:132 "(empty)"
    CHECK(FormatValue(Value::VARCHAR("")) == "(empty)");
    // test/sql/function/string/like_unicode.test:24 "true" and :39 "false";
    // DuckDB's runner prints 1 and 0 and takes either spelling
    CHECK(FormatValue(Value::BOOLEAN(true)) == "1");
    CHECK(FormatValue(Value::BOOLEAN(false)) == "0");
    CHECK(ValueMatches(FormatValue(Value::BOOLEAN(true)), "true", ColumnKind::Bool, 'I'));
    CHECK(ValueMatches(FormatValue(Value::BOOLEAN(false)), "false", ColumnKind::Bool, 'I'));
    CHECK_FALSE(ValueMatches(FormatValue(Value::BOOLEAN(false)), "true", ColumnKind::Bool, 'I'));
    // test/sql/setops/test_pg_union.test:189-192, DOUBLE values of FLOAT8_TBL
    CHECK(FormatValue(Value::DOUBLE(-1.2345678901234e+200)) == "-1.2345678901234e+200");
    CHECK(FormatValue(Value::DOUBLE(-1004.3)) == "-1004.3");
    CHECK(FormatValue(Value::DOUBLE(-34.84)) == "-34.84");
    CHECK(FormatValue(Value::DOUBLE(-1.2345678901234e-200)) == "-1.2345678901234e-200");
    // test/sql/setops/test_pg_union.test:91 "1.0" and :105 "2.0"
    CHECK(FormatValue(Value::DOUBLE(1.0)) == "1.0");
    CHECK(FormatValue(Value::DOUBLE(2.0)) == "2.0");
    // test/sql/aggregate/aggregates/binning.test:163 "-5e+307" and :164 "-0.0"
    CHECK(FormatValue(Value::DOUBLE(-5e307)) == "-5e+307");
    CHECK(FormatValue(Value::DOUBLE(-0.0)) == "-0.0");
    // test/sql/types/float/nan_ordering.test:75 "0.0"
    CHECK(FormatValue(Value::DOUBLE(0.0)) == "0.0");
    // test/sql/types/float/nan_aggregate.test:24 "inf", :29 "-inf", :34 "nan"
    CHECK(FormatValue(Value::DOUBLE(std::numeric_limits<double>::infinity())) == "inf");
    CHECK(FormatValue(Value::DOUBLE(-std::numeric_limits<double>::infinity())) == "-inf");
    CHECK(FormatValue(Value::DOUBLE(std::numeric_limits<double>::quiet_NaN())) == "nan");
    // fmt's switch to an exponent at 1e16 and below 1e-4 (DuckDB's vendored
    // fmt 6.1.2, float_writer), and a FLOAT's own shortest digits
    CHECK(FormatValue(Value::DOUBLE(1e15)) == "1000000000000000.0");
    CHECK(FormatValue(Value::DOUBLE(1e16)) == "1e+16");
    CHECK(FormatValue(Value::DOUBLE(0.0001)) == "0.0001");
    CHECK(FormatValue(Value::DOUBLE(0.00001)) == "1e-05");
    CHECK(FormatValue(Value::FLOAT(0.1f)) == "0.1");
}

TEST_CASE("slt: numbers compare as DuckDB's runner does, type for type") {
    // test/sql/function/numeric/test_round.test:24 "42.120000" for a DOUBLE
    CHECK(ValueMatches("42.12", "42.120000", ColumnKind::Float, 'R'));
    // within DuckDB's 1% tolerance, and outside it
    CHECK(ValueMatches("0.3333333333333333", "0.333333", ColumnKind::Float, 'R'));
    CHECK_FALSE(ValueMatches("0.34", "0.333333", ColumnKind::Float, 'R'));
    // test/sql/aggregate/group/group_by_limits.test:16 "-128\t 1": a space
    CHECK(ValueMatches("1", " 1", ColumnKind::Integer, 'I'));
    // test/sql/aggregate/aggregates/test_stddev.test:39 "85.000000" for a sum
    CHECK(ValueMatches("85", "85.000000", ColumnKind::Integer, 'R'));
    CHECK_FALSE(ValueMatches("85", "85.5", ColumnKind::Integer, 'R'));
    // DuckDB's DOUBLE text is not taken for an integer, and text is text
    CHECK_FALSE(ValueMatches("1", "1.0", ColumnKind::Integer, 'R'));
    CHECK_FALSE(ValueMatches("1", "1.0", ColumnKind::Other, 'T'));
    // test/sql/function/numeric/test_round.test:31 "42" for round(42::DOUBLE, 0)
    CHECK(ValueMatches("42.0", "42", ColumnKind::Float, 'I'));
    // MD5, for "N values hashing to": RFC 1321's test suite
    CHECK(Md5Hex("") == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(Md5Hex("abc") == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(Md5Hex("12345678901234567890123456789012345678901234567890123456789012345678901234567890") ==
          "57edf4a22be3c955ac49da2e2107b67a");
}

TEST_CASE("slt: a file that needs what the runner does not do is left out") {
    CHECK(ParseScript("require icu\n\nquery I\nSELECT 1\n----\n1\n", "a").left_out == "require icu");
    CHECK(ParseScript("statement ok\nSELECT * FROM read_csv('data/x.csv')\n", "a").left_out == "reads files");
    CHECK(ParseScript("statement ok\nATTACH ':memory:' AS m\n", "a").left_out == "attaches databases");
    CHECK(ParseScript("concurrentloop i 0 2\n\nstatement ok\nSELECT 1\n\nendloop\n", "a").left_out ==
          "directive concurrentloop");
    CHECK(ParseScript("require 64bit\n\nrequire vector_size 512\n\nstatement ok\nSELECT 1\n", "a").left_out.empty());
}

TEST_CASE("slt: loops, foreach, skipif and mode skip expand as DuckDB's runner does") {
    Script s = ParseScript("loop i 0 3\n\nstatement ok\nSELECT {i}\n\nendloop\n\n"
                           "foreach t <signed> varchar\n\nstatement ok\nSELECT '{t}'\n\nendloop\n\n"
                           "skipif duckdb\nstatement ok\nSELECT 'skipped'\n\n"
                           "onlyif duckdb\nstatement ok\nSELECT 'kept'\n\n"
                           "mode skip\n\nstatement ok\nSELECT 'skipped'\n\nmode unskip\n\n"
                           "statement ok\nPRAGMA threads=4\n\n"
                           "halt\n\nstatement ok\nSELECT 'after halt'\n",
                           "a");
    REQUIRE(s.left_out.empty());
    std::vector<std::string> sql;
    for (auto &c : s.cases) sql.push_back(c.sql);
    std::vector<std::string> want = {"SELECT 0",          "SELECT 1",          "SELECT 2",
                                     "SELECT 'tinyint'",  "SELECT 'smallint'", "SELECT 'integer'",
                                     "SELECT 'bigint'",   "SELECT 'hugeint'",  "SELECT 'varchar'",
                                     "SELECT 'kept'"};
    CHECK(sql == want);
}

#ifdef ZUGODB_SLT_BINARY
TEST_CASE("slt: a crash and a hang cost only their own file, through the driver") {
    DriverOptions options;
    options.binary = ZUGODB_SLT_BINARY;
    options.case_timeout = 2;
    options.test_hooks = true;
    options.jobs = 2;
    std::vector<std::pair<std::string, std::string>> files = {
        {FIXTURES + "crash.test", "crash.test"},
        {FIXTURES + "hang.test", "hang.test"},
        {FIXTURES + "classes.test", "classes.test"},
    };
    auto runs = RunFiles(files, options);
    REQUIRE(runs.size() == 3);
    auto outcomes = [](const FileRun &run) {
        std::vector<Outcome> o;
        for (auto &r : run.results) o.push_back(r.outcome);
        return o;
    };
    CHECK(outcomes(runs[0]) == std::vector<Outcome>{Outcome::Passed, Outcome::Passed, Outcome::Crashed,
                                                    Outcome::NotRun, Outcome::NotRun});
    CHECK(runs[0].lines[2] == 11);
    CHECK(outcomes(runs[1]) == std::vector<Outcome>{Outcome::Passed, Outcome::Hung, Outcome::NotRun});
    // the run went on: the third file ran whole, as in the process
    Script classes = ParseFile(FIXTURES + "classes.test", "classes.test");
    auto in_process = RunInProcess(classes);
    REQUIRE(runs[2].results.size() == in_process.size());
    for (size_t k = 0; k < in_process.size(); k++) {
        CAPTURE(runs[2].lines[k]);
        CHECK(runs[2].results[k].outcome == in_process[k].outcome);
    }
    // the outputs list the crash and the hang with their SQL
    std::string failures = FailuresTsv(runs);
    CHECK(failures.find("crash.test\t11\tcrashed\t-- zugodb_slt: crash\n") != std::string::npos);
    CHECK(failures.find("hang.test\t8\thung\t-- zugodb_slt: hang\n") != std::string::npos);
    std::string totals = TotalsTsv(runs);
    CHECK(totals.find("crashed\t1\t1\n") != std::string::npos);
    CHECK(totals.find("hung\t1\t1\n") != std::string::npos);
}

TEST_CASE("slt: the fixtures at thread caps 1 and 2 give the same classes, by the driver") {
    DriverOptions options;
    options.binary = ZUGODB_SLT_BINARY;
    options.case_timeout = 2;
    options.test_hooks = true;
    options.jobs = 3;
    std::vector<std::pair<std::string, std::string>> files = {
        {FIXTURES + "classes.test", "classes.test"},
        {FIXTURES + "crash.test", "crash.test"},
        {FIXTURES + "hang.test", "hang.test"},
    };
    options.thread_cap = 1;
    auto one = RunFiles(files, options);
    options.thread_cap = 2;
    auto two = RunFiles(files, options);
    CHECK(ThreadDiffTsv(one, 1, two, 2) == "file\tline\tcap 1\tcap 2\tsql\n");
    // and a difference is reported
    two[0].results[0].outcome = Outcome::Wrong;
    CHECK(ThreadDiffTsv(one, 1, two, 2).find("classes.test\t6\tpassed\twrong\t") != std::string::npos);
}
#endif
