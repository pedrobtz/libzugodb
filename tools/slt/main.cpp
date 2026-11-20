// zugodb_slt: the conformance runner's command line.
//
//   zugodb_slt run --corpus DIR --subset FILE --out DIR [--caps 1,2]
//              [--jobs N] [--timeout S] [--max-memory-mib M] [--verbose]
//       Runs every file of the subset (paths relative to DIR), one child
//       process per file, at each thread cap, and writes counts.tsv,
//       totals.tsv, failures.tsv and details.tsv (at the first cap) and
//       threads.tsv (the cases whose result differs between caps) to DIR.
//   zugodb_slt file PATH [--cap N] [--timeout S] [--in-process]
//       Runs one file and prints each case that did not pass.
//   zugodb_slt --child PATH CAP [--test-hooks]
//       What the driver runs for one file.

#include "slt.hpp"

#include "slothdb/common/parallel.hpp"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace zugodb::slt;

// Under AddressSanitizer (the conformance job's build): a leak at exit is
// not a crash of the case, and LeakSanitizer runs on Linux only.
extern "C" const char *__asan_default_options() { return "detect_leaks=0"; }

namespace {

int Usage() {
    std::fprintf(stderr,
                 "usage: zugodb_slt run --corpus DIR --subset FILE --out DIR [--caps 1,2] [--jobs N] "
                 "[--timeout S] [--max-memory-mib M] [--verbose]\n"
                 "       zugodb_slt file PATH [--cap N] [--timeout S] [--in-process]\n");
    return 2;
}

std::string SelfPath(const char *argv0) {
    char buf[PATH_MAX];
    if (::realpath(argv0, buf)) return buf;
    return argv0;
}

bool WriteFile(const std::string &path, const std::string &content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
    return static_cast<bool>(out);
}

struct Args {
    std::map<std::string, std::string> opts;
    std::vector<std::string> positional;
};

Args ParseArgs(int argc, char **argv, int from) {
    Args a;
    for (int i = from; i < argc; i++) {
        std::string s = argv[i];
        if (s.rfind("--", 0) == 0) {
            bool flag = s == "--verbose" || s == "--in-process" || s == "--test-hooks";
            if (flag || i + 1 >= argc) a.opts[s] = "1";
            else a.opts[s] = argv[++i];
        } else {
            a.positional.push_back(s);
        }
    }
    return a;
}

std::vector<unsigned> ParseCaps(const std::string &s) {
    std::vector<unsigned> caps;
    std::stringstream in(s);
    std::string item;
    while (std::getline(in, item, ','))
        if (!item.empty()) caps.push_back(static_cast<unsigned>(std::stoul(item)));
    return caps;
}

int RunSubset(const Args &a, const std::string &self) {
    auto get = [&](const char *k, const std::string &def = "") {
        auto it = a.opts.find(k);
        return it == a.opts.end() ? def : it->second;
    };
    std::string corpus = get("--corpus"), subset = get("--subset"), out = get("--out");
    if (corpus.empty() || subset.empty() || out.empty()) return Usage();
    std::vector<std::pair<std::string, std::string>> files;
    std::ifstream in(subset);
    if (!in) {
        std::fprintf(stderr, "cannot read %s\n", subset.c_str());
        return 2;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        files.emplace_back(corpus + "/" + line, line);
    }
    DriverOptions options;
    options.binary = get("--binary", self);
    unsigned hw = std::thread::hardware_concurrency();
    options.jobs = static_cast<unsigned>(std::stoul(get("--jobs", std::to_string(hw ? hw : 2))));
    options.case_timeout = std::stod(get("--timeout", "30"));
    options.max_resident_mib = std::stoull(get("--max-memory-mib", "2048"));
    options.verbose = a.opts.count("--verbose") > 0;
    options.test_hooks = a.opts.count("--test-hooks") > 0;
    auto caps = ParseCaps(get("--caps", "1,2"));
    if (caps.empty()) return Usage();
    std::vector<std::vector<FileRun>> runs;
    for (unsigned cap : caps) {
        options.thread_cap = cap;
        std::fprintf(stderr, "running %zu files at thread cap %u\n", files.size(), cap);
        runs.push_back(RunFiles(files, options));
    }
    bool ok = WriteFile(out + "/counts.tsv", CountsTsv(runs[0])) &&
              WriteFile(out + "/totals.tsv", TotalsTsv(runs[0])) &&
              WriteFile(out + "/failures.tsv", FailuresTsv(runs[0])) &&
              WriteFile(out + "/details.tsv", DetailsTsv(runs[0]));
    std::string diffs;
    for (size_t k = 1; k < runs.size(); k++) diffs += ThreadDiffTsv(runs[0], caps[0], runs[k], caps[k]);
    if (runs.size() == 1) diffs = ThreadDiffTsv(runs[0], caps[0], runs[0], caps[0]);
    ok = ok && WriteFile(out + "/threads.tsv", diffs);
    if (!ok) {
        std::fprintf(stderr, "cannot write to %s\n", out.c_str());
        return 2;
    }
    std::fputs(TotalsTsv(runs[0]).c_str(), stderr);
    return 0;
}

int RunOneFile(const Args &a, const std::string &self) {
    if (a.positional.size() != 1) return Usage();
    std::string path = a.positional[0];
    auto get = [&](const char *k, const std::string &def) {
        auto it = a.opts.find(k);
        return it == a.opts.end() ? def : it->second;
    };
    unsigned cap = static_cast<unsigned>(std::stoul(get("--cap", "1")));
    bool hooks = a.opts.count("--test-hooks") > 0;
    std::vector<FileRun> runs;
    if (a.opts.count("--in-process")) {
        zugodb::SetThreadCap(cap);
        Script script = ParseFile(path, path, hooks);
        FileRun run;
        run.file = path;
        run.left_out = script.left_out;
        for (auto &c : script.cases)
            if (c.kind != Case::Kind::Restart) run.lines.push_back(c.line), run.sqls.push_back(c.sql);
        run.results.resize(run.lines.size());
        RunScript(script, hooks, nullptr, [&](size_t k, const CaseResult &r) { run.results[k] = r; });
        runs.push_back(std::move(run));
    } else {
        DriverOptions options;
        options.binary = get("--binary", self);
        options.thread_cap = cap;
        options.case_timeout = std::stod(get("--timeout", "30"));
        options.max_resident_mib = std::stoull(get("--max-memory-mib", "2048"));
        options.verbose = true;
        options.test_hooks = hooks;
        runs = RunFiles({{path, path}}, options);
    }
    auto &run = runs[0];
    if (!run.left_out.empty()) {
        std::printf("left out: %s\n", run.left_out.c_str());
        return 0;
    }
    std::map<Outcome, size_t> n;
    for (size_t k = 0; k < run.results.size(); k++) {
        auto &r = run.results[k];
        n[r.outcome]++;
        if (r.outcome == Outcome::Passed || r.outcome == Outcome::NotRun) continue;
        std::printf("%s:%d: %s: %s\n    %s\n", path.c_str(), run.lines[k], OutcomeName(r.outcome),
                    r.detail.c_str(), run.sqls[k].c_str());
    }
    std::printf("%zu cases:", run.results.size());
    for (auto &[o, count] : n) std::printf(" %s %zu", OutcomeName(o), count);
    std::printf("\n");
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return Usage();
    std::string cmd = argv[1];
    std::string self = SelfPath(argv[0]);
    if (cmd == "--child") {
        if (argc < 4) return Usage();
        bool hooks = argc > 4 && std::string(argv[4]) == "--test-hooks";
        return ChildMain(argv[2], static_cast<unsigned>(std::stoul(argv[3])), hooks);
    }
    try {
        if (cmd == "run") return RunSubset(ParseArgs(argc, argv, 2), self);
        if (cmd == "file") return RunOneFile(ParseArgs(argc, argv, 2), self);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "zugodb_slt: %s\n", e.what());
        return 2;
    }
    return Usage();
}
