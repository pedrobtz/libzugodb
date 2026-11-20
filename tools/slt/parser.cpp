// Parsing and expanding a sqllogictest file (design L14).
//
// The dialect is DuckDB's at the pinned tag (test/sqlite/sqllogic_parser.cpp
// and sqllogic_test_runner.cpp): records separated by empty lines, `#`
// comments, `statement ok|error|maybe`, `query <types> [sort] [label]`
// with `----` before the expected values, and the directives below. A
// file that needs anything else is left out with the reason.

#include "slt.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>

namespace zugodb::slt {

namespace {

std::string Lower(std::string s) {
    for (auto &ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

std::string Trim(const std::string &s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

std::vector<std::string> Split(const std::string &s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : s) {
        if (ch == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += ch;
        }
    }
    out.push_back(cur);
    return out;
}

std::vector<std::string> Tokenize(const std::string &line) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : line) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!cur.empty()) out.push_back(cur), cur.clear();
        } else {
            cur += ch;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool EmptyOrComment(const std::string &line) { return line.empty() || line[0] == '#'; }

void ReplaceAll(std::string &text, const std::string &from, const std::string &to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

// A loop or a record, before expansion.
struct Node {
    bool is_loop = false;
    // loop
    std::string iterator;
    std::vector<std::string> tokens;  // foreach; empty for a numeric loop
    int start = 0, end = 0;
    std::vector<std::unique_ptr<Node>> body;
    // record
    Case record;
};

struct ActiveLoop {
    std::string iterator;
    std::string value;
};

// DuckDB's `require` arguments that hold for this runner (L14): the
// platform ones that are true on 64-bit Linux and macOS alike, and the ones
// about DuckDB's own build or test modes, which do not change its answers.
bool RequireHolds(const std::vector<std::string> &params) {
    if (params.empty()) return false;
    std::string p = Lower(params[0]);
    if (p == "64bit") return sizeof(void *) == 8;
    if (p == "notwindows" || p == "notmingw" || p == "notmusl" || p == "nothreadsan") return true;
    if (p == "noforcestorage" || p == "no_force_storage" || p == "skip_reload" ||
        p == "no_alternative_verify" || p == "strinline" || p == "no_extension_autoloading")
        return true;
    if (p == "vector_size" && params.size() == 2) {
        // DuckDB's answers were recorded at its standard vector size, 2048.
        try {
            return std::stoi(params[1]) <= 2048;
        } catch (...) {
            return false;
        }
    }
    return false;
}

// Patterns of SQL that reads or writes files, or needs DuckDB's test
// directories: a file with one is left out (design §5).
const char *FileAccessReason(const std::string &sql) {
    static const std::vector<std::pair<std::regex, const char *>> patterns = [] {
        std::vector<std::pair<std::regex, const char *>> v;
        auto add = [&](const char *re, const char *why) {
            v.emplace_back(std::regex(re, std::regex::icase | std::regex::ECMAScript), why);
        };
        add(R"(\b(read_csv|read_csv_auto|read_parquet|parquet_scan|read_json|read_json_auto|read_ndjson|read_json_objects|read_text|read_blob|sniff_csv|parquet_metadata|parquet_schema|glob)\s*\()",
            "reads files");
        add(R"(['"](data|test|benchmark|extension|third_party)/)", "reads files");
        add(R"(\.(csv|tsv|parquet|json|jsonl|ndjson|db|duckdb|gz|zst|arrow)['"])", "reads files");
        add(R"(\bcopy\b)", "reads files");
        add(R"(__TEST_DIR__|\{TEST_DIR\}|\{DATA_DIR\}|__WORKING_DIRECTORY__|\{WORKING_DIRECTORY\}|__BUILD_DIRECTORY__|\{BUILD_DIRECTORY\}|\{TEMP_DIR\})",
            "reads files");
        add(R"(\b(attach|detach)\b)", "attaches databases");
        add(R"(\b(export|import)\s+database\b)", "reads files");
        add(R"(\b(install|load)\s+[a-z_'"]+)", "loads extensions");
        return v;
    }();
    for (auto &[re, why] : patterns)
        if (std::regex_search(sql, re)) return why;
    return nullptr;
}

// DuckDB settings that change how DuckDB computes, not what it answers:
// the tests set them to reach other code paths with the same answers. A
// `statement ok` that sets one is not run and not counted (L17).
bool IsNoOpSetting(const std::string &sql) {
    static const std::regex re(
        R"(^\s*(pragma|set|reset)\s+((global|session|local)\s+)?)"
        R"((threads|verify_parallelism|disable_verify_parallelism|enable_verification|disable_verification|)"
        R"(verify_external|disable_verify_external|verify_serializer|disable_verify_serializer|)"
        R"(enable_force_parallelism|disable_force_parallelism|force_parallelism|)"
        R"(debug_force_external|debug_force_no_cross_product|debug_verify_vector|debug_verify_serializer|)"
        R"(debug_verify_column_bindings|debug_window_mode|debug_asof_iejoin|debug_disable_optimizer|)"
        R"(disabled_optimizers|enable_optimizer|disable_optimizer|)"
        R"(merge_join_threshold|nested_loop_join_threshold|asof_loop_join_threshold|prefer_range_joins|)"
        R"(delim_join_as_cte|dynamic_or_filter_threshold|pivot_filter_threshold|perfect_ht_threshold|)"
        R"(memory_limit|max_memory|max_temp_directory_size|enable_profiling|disable_profiling|)"
        R"(profiling_mode|profiling_output|profiling_renderer_settings|enable_progress_bar|)"
        R"(disable_progress_bar|enable_object_cache|disable_object_cache|explain_output|)"
        R"(enable_verify_parallelism|verify_fetch_row|enable_print_progress_bar|disable_print_progress_bar))"
        R"(\b[^;]*;?\s*$)",
        std::regex::icase | std::regex::ECMAScript);
    return std::regex_search(sql, re);
}

class Parser {
public:
    Parser(const std::string &text, const std::string &name, bool test_hooks)
        : name_(name), test_hooks_(test_hooks) {
        std::string line;
        std::istringstream in(text);
        while (std::getline(in, line)) {
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
            lines_.push_back(line);
        }
    }

    Script Run() {
        Script script;
        script.name = name_;
        for (auto &l : lines_)
            if (l.rfind("statement ", 0) == 0 || l.rfind("query ", 0) == 0) script.static_cases++;
        std::vector<std::unique_ptr<Node>> top;
        std::vector<std::vector<std::unique_ptr<Node>> *> stack{&top};
        std::vector<std::unique_ptr<Node>> open_loops;
        ParseRecords(stack, open_loops);
        if (left_out_.empty() && open_loops.size() > 0) left_out_ = "missing endloop";
        script.regex_cases = regex_cases_;
        if (!left_out_.empty()) {
            script.left_out = left_out_;
            return script;
        }
        std::vector<ActiveLoop> active;
        Expand(top, active, script.cases);
        if (!left_out_.empty()) {
            script.left_out = left_out_;
            script.cases.clear();
        }
        return script;
    }

private:
    void LeaveOut(const std::string &why) {
        if (left_out_.empty()) left_out_ = why;
    }

    bool Has(size_t i) const { return i < lines_.size(); }

    void ParseRecords(std::vector<std::vector<std::unique_ptr<Node>> *> &stack,
                      std::vector<std::unique_ptr<Node>> &open_loops) {
        size_t i = 0;
        int skip_level = 0;
        bool seen = false;
        while (left_out_.empty()) {
            // DuckDB's NextStatement: the rest of the previous record goes
            if (seen)
                while (Has(i) && !EmptyOrComment(lines_[i])) i++;
            seen = true;
            while (Has(i) && EmptyOrComment(lines_[i])) i++;
            if (!Has(i)) break;
            auto tok = Tokenize(lines_[i]);
            bool skip = false;
            while (!tok.empty() && (tok[0] == "skipif" || tok[0] == "onlyif")) {
                bool skip_if = tok[0] == "skipif";
                if (tok.size() < 2) return LeaveOut("malformed skipif/onlyif");
                std::string system = Lower(tok[1]);
                if (system.find_first_of("=<>") != std::string::npos)
                    return LeaveOut("skipif/onlyif on a loop variable");
                bool ours = system == "duckdb";
                if (ours == skip_if) {
                    skip = true;
                    break;
                }
                i++;
                if (!Has(i)) return;
                tok = Tokenize(lines_[i]);
            }
            auto skip_record = [&] {
                while (Has(i) && !EmptyOrComment(lines_[i])) i++;
            };
            if (skip || tok.empty()) {
                skip_record();
                continue;
            }
            const std::string &cmd = tok[0];
            if (skip_level > 0 && cmd != "mode") {
                skip_record();
                continue;
            }
            if (cmd == "statement") {
                auto node = std::make_unique<Node>();
                Case &c = node->record;
                c.kind = Case::Kind::Statement;
                c.line = static_cast<int>(i) + 1;
                if (tok.size() < 2) return LeaveOut("malformed statement");
                if (tok[1] == "ok") c.expect = Expect::Ok;
                else if (tok[1] == "error") c.expect = Expect::Error;
                else if (tok[1] == "maybe") c.expect = Expect::Maybe;
                else return LeaveOut("statement " + tok[1]);
                if (tok.size() > 2) return LeaveOut("named connections");
                i++;
                c.sql = ExtractStatement(i);
                if (c.sql.empty()) return LeaveOut("empty statement");
                if (!CheckSql(c.sql)) return;
                if (c.expect == Expect::Ok && IsNoOpSetting(c.sql)) continue;
                if (Has(i) && lines_[i] == "----") {
                    i++;
                    std::vector<std::string> err;
                    while (Has(i) && !lines_[i].empty()) err.push_back(lines_[i++]);
                    for (size_t k = 0; k < err.size(); k++) c.expected_error += (k ? "\n" : "") + err[k];
                }
                stack.back()->push_back(std::move(node));
            } else if (cmd == "query") {
                auto node = std::make_unique<Node>();
                Case &c = node->record;
                c.kind = Case::Kind::Query;
                c.line = static_cast<int>(i) + 1;
                if (tok.size() < 2) return LeaveOut("malformed query");
                c.types = tok[1];
                for (char t : c.types)
                    if (t != 'I' && t != 'T' && t != 'R') return LeaveOut("query type " + c.types);
                if (tok.size() > 2) {
                    if (tok[2] == "nosort") c.sort = SortMode::None;
                    else if (tok[2] == "rowsort" || tok[2] == "sort") c.sort = SortMode::Row;
                    else if (tok[2] == "valuesort") c.sort = SortMode::Value;
                    else return LeaveOut("named connections");
                }
                if (tok.size() > 3) c.label = tok[3];
                c.hash_threshold = hash_threshold_;
                i++;
                c.sql = ExtractStatement(i);
                if (!CheckSql(c.sql)) return;
                if (Has(i) && lines_[i] == "----") i++;
                while (Has(i) && !lines_[i].empty()) c.values.push_back(lines_[i++]);
                bool regex = false;
                for (auto &v : c.values) {
                    if (v.rfind("<FILE>:", 0) == 0) return LeaveOut("results in a file");
                    // A regex in any column: DuckDB's runner matches it
                    // against DuckDB's own text (an EXPLAIN plan, ...),
                    // which no other engine gives. The query is left out
                    // alone, not its file: a query changes nothing the
                    // file's later cases read (design L20).
                    std::istringstream fields(v);
                    std::string f;
                    while (std::getline(fields, f, '\t'))
                        if (f.rfind("<REGEX>:", 0) == 0 || f.rfind("<!REGEX>:", 0) == 0) regex = true;
                }
                if (regex) {
                    regex_cases_++;
                    continue;
                }
                stack.back()->push_back(std::move(node));
            } else if (cmd == "hash-threshold") {
                if (tok.size() != 2) return LeaveOut("malformed hash-threshold");
                try {
                    hash_threshold_ = std::stoi(tok[1]);
                } catch (...) {
                    return LeaveOut("malformed hash-threshold");
                }
                i++;
            } else if (cmd == "halt") {
                break;
            } else if (cmd == "mode") {
                if (tok.size() < 2) return LeaveOut("malformed mode");
                if (tok[1] == "skip") skip_level++;
                else if (tok[1] == "unskip") skip_level--;
                // other modes (output_result, debug, ...) only print
                i++;
            } else if (cmd == "loop" || cmd == "foreach") {
                auto node = std::make_unique<Node>();
                node->is_loop = true;
                if (cmd == "loop") {
                    if (tok.size() != 4) return LeaveOut("malformed loop");
                    node->iterator = tok[1];
                    try {
                        node->start = std::stoi(tok[2]);
                        node->end = std::stoi(tok[3]);
                    } catch (...) {
                        return LeaveOut("malformed loop");
                    }
                } else {
                    if (tok.size() < 3) return LeaveOut("malformed foreach");
                    node->iterator = tok[1];
                    for (size_t k = 2; k < tok.size(); k++)
                        if (!ForEachToken(tok[k], node->tokens)) return;
                }
                Node *raw = node.get();
                stack.back()->push_back(std::move(node));
                stack.push_back(&raw->body);
                open_loops.push_back(nullptr);
                i++;
            } else if (cmd == "endloop") {
                if (open_loops.empty()) return LeaveOut("endloop without loop");
                open_loops.pop_back();
                stack.pop_back();
                i++;
            } else if (cmd == "require") {
                std::vector<std::string> params(tok.begin() + 1, tok.end());
                if (!RequireHolds(params)) return LeaveOut("require " + (params.empty() ? "" : params[0]));
                i++;
            } else if (cmd == "load") {
                if (tok.size() > 1) return LeaveOut("a database file");
                // `load` with no path is a fresh in-memory database
                auto node = std::make_unique<Node>();
                node->record.kind = Case::Kind::Restart;
                node->record.line = static_cast<int>(i) + 1;
                stack.back()->push_back(std::move(node));
                i++;
            } else if (cmd == "restart") {
                auto node = std::make_unique<Node>();
                node->record.kind = Case::Kind::Restart;
                node->record.line = static_cast<int>(i) + 1;
                stack.back()->push_back(std::move(node));
                i++;
            } else {
                // concurrentloop, concurrentforeach, set, reset, include, unzip,
                // sleep, reconnect, continue, test-env, require-env, tags, ...
                return LeaveOut("directive " + cmd);
            }
        }
    }

    bool CheckSql(const std::string &sql) {
        if (test_hooks_ && (sql == kHookCrash || sql == kHookHang)) return true;
        if (const char *why = FileAccessReason(sql)) {
            LeaveOut(why);
            return false;
        }
        return true;
    }

    // DuckDB's ExtractStatement: lines until an empty line, a comment or ----.
    std::string ExtractStatement(size_t &i) {
        std::string sql;
        bool first = true;
        while (Has(i) && !EmptyOrComment(lines_[i])) {
            if (lines_[i] == "----") break;
            if (!first) sql += "\n";
            sql += lines_[i];
            first = false;
            i++;
        }
        return sql;
    }

    // DuckDB's ForEachTokenReplace, for the groups the subset uses.
    bool ForEachToken(const std::string &param, std::vector<std::string> &out) {
        std::string t = Lower(Trim(param));
        if (t.empty()) return true;
        if (t[0] == '!') {
            auto it = std::find(out.begin(), out.end(), param.substr(1));
            if (it != out.end()) {
                out.erase(it);
                return true;
            }
            out.push_back(param);
            return true;
        }
        if (t[0] != '<') {
            out.push_back(param);
            return true;
        }
        bool all = t == "<alltypes>";
        bool numeric = all || t == "<numeric>";
        bool integral = numeric || t == "<integral>";
        bool is_signed = integral || t == "<signed>";
        bool is_unsigned = integral || t == "<unsigned>";
        if (t == "<compression>" || t == "<all_types_columns>") {
            LeaveOut("foreach " + t);
            return false;
        }
        if (!is_signed && !is_unsigned) {
            out.push_back(param);  // not a group: the token itself, as in DuckDB
            return true;
        }
        if (is_signed)
            for (auto s : {"tinyint", "smallint", "integer", "bigint", "hugeint"}) out.push_back(s);
        if (is_unsigned)
            for (auto s : {"utinyint", "usmallint", "uinteger", "ubigint", "uhugeint"}) out.push_back(s);
        if (numeric)
            for (auto s : {"float", "double"}) out.push_back(s);
        if (all)
            for (auto s : {"bool", "interval", "varchar"}) out.push_back(s);
        return true;
    }

    static std::string Substitute(std::string text, const std::vector<ActiveLoop> &active) {
        for (auto &loop : active) {
            if (loop.iterator.find(',') != std::string::npos) {
                auto names = Split(loop.iterator, ',');
                auto values = Split(loop.value, ',');
                for (size_t k = 0; k < names.size() && k < values.size(); k++) {
                    ReplaceAll(text, "${" + names[k] + "}", values[k]);
                    ReplaceAll(text, "{" + names[k] + "}", values[k]);
                }
            } else {
                ReplaceAll(text, "${" + loop.iterator + "}", loop.value);
                ReplaceAll(text, "{" + loop.iterator + "}", loop.value);
            }
        }
        return text;
    }

    void Expand(const std::vector<std::unique_ptr<Node>> &nodes, std::vector<ActiveLoop> &active,
                std::vector<Case> &out) {
        for (auto &node : nodes) {
            if (!left_out_.empty()) return;
            if (!node->is_loop) {
                Case c = node->record;
                if (c.kind != Case::Kind::Restart) {
                    c.sql = Substitute(c.sql, active);
                    if (c.sql.find("${") != std::string::npos) return LeaveOut("variables in SQL");
                }
                out.push_back(std::move(c));
                if (out.size() > 2000000) return LeaveOut("too many cases");
                continue;
            }
            active.push_back({node->iterator, ""});
            if (node->tokens.empty()) {
                // DuckDB runs a loop's body once even if start >= end.
                int idx = node->start;
                do {
                    active.back().value = std::to_string(idx);
                    Expand(node->body, active, out);
                    idx++;
                } while (idx < node->end && left_out_.empty());
            } else {
                for (auto &t : node->tokens) {
                    active.back().value = t;
                    Expand(node->body, active, out);
                }
            }
            active.pop_back();
        }
    }

    std::string name_;
    bool test_hooks_;
    std::vector<std::string> lines_;
    std::string left_out_;
    int hash_threshold_ = 0;
    size_t regex_cases_ = 0;
};

} // namespace

size_t Script::CaseCount() const {
    size_t n = 0;
    for (auto &c : cases)
        if (c.kind != Case::Kind::Restart) n++;
    return n;
}

Script ParseScript(const std::string &text, const std::string &name, bool test_hooks) {
    Parser parser(text, name, test_hooks);
    return parser.Run();
}

Script ParseFile(const std::string &path, const std::string &name, bool test_hooks) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        Script s;
        s.name = name;
        s.left_out = "file not found";
        return s;
    }
    std::stringstream buf;
    buf << in.rdbuf();
    return ParseScript(buf.str(), name, test_hooks);
}

} // namespace zugodb::slt
