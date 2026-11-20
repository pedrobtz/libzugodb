// The driver: one child process per file, a time limit per case, and the
// outputs (design §5 and L16). POSIX only; the conformance job runs on
// Linux and macOS.
//
// The child runs `zugodb_slt --child <file> <cap>` and writes, on fd 3,
// "B <k>" before case k and "E <k> <outcome> <detail>" after
// it, then "D" at the end. The driver parses the same file, so it knows
// every case's line and SQL. A child that dies leaves the case it began
// crashed; one that makes no progress for the time limit is killed and
// that case is hung. The cases after either are not run.

#include "slt.hpp"

#include "slothdb/common/parallel.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <map>
#include <poll.h>
#include <set>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libproc.h>
#endif

namespace zugodb::slt {

namespace {

using Clock = std::chrono::steady_clock;

std::string OneLine(const std::string &s, size_t max) {
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

void WriteAll(int fd, const std::string &s) {
    size_t off = 0;
    while (off < s.size()) {
        ssize_t n = ::write(fd, s.data() + off, s.size() - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            return;
        }
        off += static_cast<size_t>(n);
    }
}

const char *SignalName(int sig) {
    switch (sig) {
    case SIGSEGV: return "SIGSEGV";
    case SIGABRT: return "SIGABRT";
    case SIGBUS: return "SIGBUS";
    case SIGFPE: return "SIGFPE";
    case SIGILL: return "SIGILL";
    case SIGKILL: return "SIGKILL";
    default: return "a signal";
    }
}

// The resident memory of a child, in bytes, or 0 if unknown.
uint64_t ResidentBytes(pid_t pid) {
#ifdef __APPLE__
    proc_taskinfo info;
    if (::proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &info, sizeof(info)) == static_cast<int>(sizeof(info)))
        return info.pti_resident_size;
    return 0;
#else
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/statm", static_cast<int>(pid));
    FILE *f = std::fopen(path, "r");
    if (!f) return 0;
    unsigned long long size = 0, resident = 0;
    int n = std::fscanf(f, "%llu %llu", &size, &resident);
    std::fclose(f);
    if (n != 2) return 0;
    return resident * static_cast<uint64_t>(::sysconf(_SC_PAGESIZE));
#endif
}

struct Child {
    size_t run_index;
    pid_t pid = -1;
    int fd = -1;
    std::string buffer;
    long begun = -1;  // the case begun and not yet ended
    size_t ended = 0;
    bool done = false;
    bool killed = false;
    std::string kill_reason;
    Clock::time_point last_progress;
};

void HandleLine(Child &child, FileRun &run, const std::string &line) {
    if (line.empty()) return;
    child.last_progress = Clock::now();
    if (line == "D") {
        child.done = true;
        return;
    }
    std::istringstream in(line);
    std::string tag;
    size_t index;
    in >> tag >> index;
    if (!in || index >= run.results.size()) return;
    if (tag == "B") {
        child.begun = static_cast<long>(index);
    } else if (tag == "E") {
        std::string outcome;
        in >> outcome;
        std::string detail;
        std::getline(in, detail);
        if (!detail.empty() && detail[0] == ' ') detail.erase(0, 1);
        CaseResult &r = run.results[index];
        if (!ParseOutcome(outcome, r.outcome)) r.outcome = Outcome::Crashed;
        r.detail = detail;
        child.begun = -1;
        child.ended = index + 1;
    }
}

void Finish(Child &child, FileRun &run, int status, bool hung, const std::string &kill_reason) {
    if (child.done && !hung && WIFEXITED(status) && WEXITSTATUS(status) == 0) return;
    Outcome what = hung ? Outcome::Hung : Outcome::Crashed;
    std::string how;
    if (hung) how = kill_reason;
    else if (WIFSIGNALED(status)) how = std::string("killed by ") + SignalName(WTERMSIG(status));
    else if (WIFEXITED(status)) how = "exited with status " + std::to_string(WEXITSTATUS(status));
    else how = "ended";
    if (child.done && !hung) {
        // every case ended, then the child crashed on the way out
        run.crash_note = how + ", after its last case";
        if (!run.results.empty()) {
            run.results.back().outcome = Outcome::Crashed;
            run.results.back().detail = run.crash_note;
        }
        return;
    }
    size_t at = child.begun >= 0 ? static_cast<size_t>(child.begun) : child.ended;
    run.crash_note = how;
    if (at < run.results.size()) {
        run.results[at].outcome = what;
        run.results[at].detail = how;
        for (size_t k = at + 1; k < run.results.size(); k++) run.results[k] = CaseResult{};
    } else if (!run.results.empty()) {
        run.results.back().outcome = what;
        run.results.back().detail = how + ", after its last case";
    }
}

} // namespace

int ChildMain(const std::string &path, unsigned thread_cap, bool test_hooks) {
    SetThreadCap(thread_cap);
    Script script = ParseFile(path, path, test_hooks);
    if (!script.left_out.empty()) {
        WriteAll(3, "D\n");
        return 0;
    }
    RunScript(
        script, test_hooks, [](size_t k) { WriteAll(3, "B " + std::to_string(k) + "\n"); },
        [](size_t k, const CaseResult &r) {
            WriteAll(3, "E " + std::to_string(k) + " " + OutcomeName(r.outcome) + " " +
                            OneLine(r.detail, 300) + "\n");
        });
    WriteAll(3, "D\n");
    return 0;
}

std::vector<FileRun> RunFiles(const std::vector<std::pair<std::string, std::string>> &files,
                              const DriverOptions &options) {
    std::vector<FileRun> runs(files.size());
    std::deque<size_t> queue;
    for (size_t i = 0; i < files.size(); i++) {
        FileRun &run = runs[i];
        run.file = files[i].second;
        Script script = ParseFile(files[i].first, files[i].second, options.test_hooks);
        if (!script.left_out.empty()) {
            run.left_out = script.left_out;
            continue;
        }
        run.regex_cases = script.regex_cases;
        for (auto &c : script.cases) {
            if (c.kind == Case::Kind::Restart) continue;
            run.lines.push_back(c.line);
            run.sqls.push_back(c.sql);
        }
        run.results.assign(run.lines.size(), CaseResult{});
        if (!run.results.empty()) queue.push_back(i);
    }

    std::vector<Child> active;
    unsigned jobs = std::max(1u, options.jobs);
    auto start = [&](size_t i) {
        int pipefd[2];
        if (::pipe(pipefd) != 0) throw std::runtime_error("pipe failed");
        std::string cap = std::to_string(options.thread_cap);
        std::vector<std::string> args = {options.binary, "--child", files[i].first, cap};
        if (options.test_hooks) args.push_back("--test-hooks");
        std::vector<char *> argv;
        for (auto &a : args) argv.push_back(const_cast<char *>(a.c_str()));
        argv.push_back(nullptr);
        pid_t pid = ::fork();
        if (pid < 0) throw std::runtime_error("fork failed");
        if (pid == 0) {
            ::close(pipefd[0]);
            if (pipefd[1] != 3) {
                ::dup2(pipefd[1], 3);
                ::close(pipefd[1]);
            }
            int devnull = ::open("/dev/null", O_RDWR);
            ::dup2(devnull, 0);
            ::dup2(devnull, 1);
            if (!options.verbose) ::dup2(devnull, 2);
            if (devnull > 3) ::close(devnull);
            ::execv(argv[0], argv.data());
            ::_exit(127);
        }
        ::close(pipefd[1]);
        ::fcntl(pipefd[0], F_SETFL, ::fcntl(pipefd[0], F_GETFL) | O_NONBLOCK);
        ::fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
        Child child;
        child.run_index = i;
        child.pid = pid;
        child.fd = pipefd[0];
        child.last_progress = Clock::now();
        active.push_back(std::move(child));
    };

    while (!queue.empty() || !active.empty()) {
        while (active.size() < jobs && !queue.empty()) {
            start(queue.front());
            queue.pop_front();
        }
        std::vector<pollfd> fds;
        for (auto &c : active) fds.push_back({c.fd, POLLIN, 0});
        ::poll(fds.data(), fds.size(), 200);
        auto now = Clock::now();
        for (size_t k = 0; k < active.size();) {
            Child &child = active[k];
            FileRun &run = runs[child.run_index];
            bool eof = false;
            char buf[65536];
            for (;;) {
                ssize_t n = ::read(child.fd, buf, sizeof(buf));
                if (n > 0) {
                    child.buffer.append(buf, static_cast<size_t>(n));
                    continue;
                }
                if (n == 0) eof = true;
                else if (errno == EINTR) continue;
                break;
            }
            size_t nl;
            while ((nl = child.buffer.find('\n')) != std::string::npos) {
                HandleLine(child, run, child.buffer.substr(0, nl));
                child.buffer.erase(0, nl + 1);
            }
            double idle = std::chrono::duration<double>(now - child.last_progress).count();
            if (!eof && !child.killed && idle > options.case_timeout) {
                ::kill(child.pid, SIGKILL);
                child.killed = true;
                child.kill_reason = "no progress within the time limit";
            }
            if (!eof && !child.killed && options.max_resident_mib > 0 &&
                ResidentBytes(child.pid) > options.max_resident_mib * 1024ull * 1024ull) {
                ::kill(child.pid, SIGKILL);
                child.killed = true;
                child.kill_reason = "memory past the limit";
            }
            if (eof || child.killed) {
                int status = 0;
                while (::waitpid(child.pid, &status, 0) < 0 && errno == EINTR) {
                }
                ::close(child.fd);
                Finish(child, run, status, child.killed, child.kill_reason);
                active.erase(active.begin() + static_cast<long>(k));
                continue;
            }
            k++;
        }
    }
    return runs;
}

namespace {

constexpr Outcome kCounted[] = {Outcome::Passed,     Outcome::Refused, Outcome::Wrong, Outcome::Accepted,
                                Outcome::Unreliable, Outcome::Crashed, Outcome::Hung,  Outcome::NotRun};

std::vector<const FileRun *> Sorted(const std::vector<FileRun> &runs) {
    std::vector<const FileRun *> out;
    for (auto &r : runs) out.push_back(&r);
    std::sort(out.begin(), out.end(), [](auto *a, auto *b) { return a->file < b->file; });
    return out;
}

} // namespace

std::string CountsTsv(const std::vector<FileRun> &runs) {
    std::ostringstream out;
    out << "file\tstatus\tcases";
    for (auto o : kCounted) out << "\t" << OutcomeName(o);
    out << "\n";
    for (auto *run : Sorted(runs)) {
        out << run->file << "\t" << (run->left_out.empty() ? "run" : "left out: " + run->left_out);
        if (!run->left_out.empty()) {
            out << "\t\t\t\t\t\t\t\t\t\n";
            continue;
        }
        std::map<Outcome, size_t> n;
        for (auto &r : run->results) n[r.outcome]++;
        out << "\t" << run->results.size();
        for (auto o : kCounted) out << "\t" << n[o];
        out << "\n";
    }
    return out.str();
}

std::string TotalsTsv(const std::vector<FileRun> &runs) {
    std::ostringstream out;
    size_t files_run = 0, cases = 0;
    std::map<Outcome, std::pair<size_t, size_t>> n;  // files, cases
    std::map<std::string, size_t> left_out;
    size_t regex_files = 0, regex_cases = 0;
    for (auto &run : runs) {
        if (!run.left_out.empty()) {
            std::string reason = run.left_out;
            left_out[reason]++;
            continue;
        }
        if (run.regex_cases > 0) {
            regex_files++;
            regex_cases += run.regex_cases;
        }
        files_run++;
        cases += run.results.size();
        std::set<Outcome> seen;
        for (auto &r : run.results) {
            n[r.outcome].second++;
            seen.insert(r.outcome);
        }
        for (auto o : seen) n[o].first++;
    }
    out << "what\tfiles\tcases\n";
    out << "run\t" << files_run << "\t" << cases << "\n";
    for (auto o : kCounted) out << OutcomeName(o) << "\t" << n[o].first << "\t" << n[o].second << "\n";
    size_t lo = 0;
    for (auto &[reason, files] : left_out) lo += files;
    out << "left out\t" << lo << "\t\n";
    for (auto &[reason, files] : left_out) out << "left out: " << reason << "\t" << files << "\t\n";
    // Queries left out of files that run (design L20).
    out << "queries left out: regex results\t" << regex_files << "\t" << regex_cases << "\n";
    return out.str();
}

std::string FailuresTsv(const std::vector<FileRun> &runs) {
    std::ostringstream out;
    out << "file\tline\tclass\tsql\n";
    for (auto *run : Sorted(runs)) {
        std::set<std::string> seen;
        for (size_t k = 0; k < run->results.size(); k++) {
            Outcome o = run->results[k].outcome;
            if (o != Outcome::Wrong && o != Outcome::Accepted && o != Outcome::Unreliable &&
                o != Outcome::Crashed && o != Outcome::Hung)
                continue;
            std::string row = run->file + "\t" + std::to_string(run->lines[k]) + "\t" + OutcomeName(o) + "\t" +
                              OneLine(run->sqls[k], 400) + "\n";
            if (seen.insert(row).second) out << row;
        }
    }
    return out.str();
}

std::string DetailsTsv(const std::vector<FileRun> &runs) {
    std::ostringstream out;
    out << "file\tline\tclass\tdetail\tsql\n";
    for (auto *run : Sorted(runs)) {
        for (size_t k = 0; k < run->results.size(); k++) {
            auto &r = run->results[k];
            if (r.outcome == Outcome::Passed || r.outcome == Outcome::NotRun) continue;
            out << run->file << "\t" << run->lines[k] << "\t" << OutcomeName(r.outcome) << "\t"
                << OneLine(r.detail, 300) << "\t" << OneLine(run->sqls[k], 400) << "\n";
        }
    }
    return out.str();
}

std::string ThreadDiffTsv(const std::vector<FileRun> &a, unsigned cap_a, const std::vector<FileRun> &b,
                          unsigned cap_b) {
    std::ostringstream out;
    out << "file\tline\tcap " << cap_a << "\tcap " << cap_b << "\tsql\n";
    std::map<std::string, const FileRun *> by_name;
    for (auto &r : b) by_name[r.file] = &r;
    for (auto *ra : Sorted(a)) {
        auto it = by_name.find(ra->file);
        if (it == by_name.end()) continue;
        const FileRun *rb = it->second;
        if (ra->left_out != rb->left_out || ra->results.size() != rb->results.size()) {
            out << ra->file << "\t\t" << ra->results.size() << " cases\t" << rb->results.size() << " cases\t\n";
            continue;
        }
        for (size_t k = 0; k < ra->results.size(); k++) {
            auto &x = ra->results[k];
            auto &y = rb->results[k];
            if (x.outcome == y.outcome) continue;
            out << ra->file << "\t" << ra->lines[k] << "\t" << OutcomeName(x.outcome) << "\t"
                << OutcomeName(y.outcome) << "\t"
                << OneLine(ra->sqls[k], 400) << "\n";
        }
    }
    return out.str();
}

} // namespace zugodb::slt
