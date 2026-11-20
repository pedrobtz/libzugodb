#include "slothdb/execution/regex.hpp"

#include "slothdb/common/exception.hpp"

#include <algorithm>

// See the header and design L24. RE2's behaviour is the reference for
// every rule here: its syntax for what is accepted or an error, and its
// leftmost-first matching, which a Pike VM gives by running the threads
// of each position in priority order and dropping those below a match.

namespace zugodb {
namespace regex {

namespace {

[[noreturn]] void Invalid(const std::string &what) {
    throw InvalidInputException("invalid regular expression: " + what);
}

[[noreturn]] void Unsupported(const std::string &what) {
    throw NotImplementedException("regular expression " + what + " is not supported");
}

using Range = std::pair<uint32_t, uint32_t>;

enum class NodeKind { Empty, Char, Any, Class, Begin, End, WordB, NotWordB, Concat, Alt, Repeat, Group };

struct Node {
    NodeKind kind = NodeKind::Empty;
    uint32_t c = 0;
    std::vector<Range> ranges;  // a Class
    bool negated = false;
    std::vector<std::unique_ptr<Node>> kids;
    int min = 0, max = 0;  // a Repeat; max -1 has no bound
    bool lazy = false;
    int cap = -1;  // a Group: its capture index, -1 for (?: )
};

using NodePtr = std::unique_ptr<Node>;

NodePtr Make(NodeKind k) {
    auto n = std::make_unique<Node>();
    n->kind = k;
    return n;
}

bool Nullable(const Node &n) {
    switch (n.kind) {
    case NodeKind::Char:
    case NodeKind::Any:
    case NodeKind::Class:
        return false;
    case NodeKind::Concat:
        for (auto &k : n.kids) {
            if (!Nullable(*k)) return false;
        }
        return true;
    case NodeKind::Alt:
        for (auto &k : n.kids) {
            if (Nullable(*k)) return true;
        }
        return false;
    case NodeKind::Repeat:
        return n.min == 0 || Nullable(*n.kids[0]);
    case NodeKind::Group:
        return Nullable(*n.kids[0]);
    default:
        return true;  // Empty and the assertions match no character
    }
}

constexpr int kMaxRepeat = 1000;  // RE2's
constexpr int kMaxDepth = 1000;
constexpr size_t kMaxProgram = 100000;

const std::vector<Range> kDigit = {{'0', '9'}};
const std::vector<Range> kWord = {{'0', '9'}, {'A', 'Z'}, {'_', '_'}, {'a', 'z'}};
const std::vector<Range> kSpace = {{'\t', '\n'}, {'\f', '\r'}, {' ', ' '}};  // RE2's \s

std::vector<Range> Negate(std::vector<Range> r) {
    std::sort(r.begin(), r.end());
    std::vector<Range> out;
    uint32_t next = 0;
    for (auto &x : r) {
        if (x.first > next) out.push_back({next, x.first - 1});
        if (x.second + 1 > next) next = x.second + 1;
    }
    if (next <= 0x10FFFF) out.push_back({next, 0x10FFFF});
    return out;
}

class Parser {
public:
    Parser(const std::string &p, bool literal) : p_(p), literal_(literal) {}

    NodePtr Parse() {
        if (literal_) {
            auto cat = Make(NodeKind::Concat);
            size_t i = 0;
            while (i < p_.size()) {
                auto ch = Make(NodeKind::Char);
                ch->c = DecodeAt(p_, i);
                cat->kids.push_back(std::move(ch));
            }
            return cat;
        }
        auto n = ParseAlt(0);
        if (pos_ < p_.size()) Invalid("unexpected ) in " + p_);  // ParseAlt stops only at )
        return n;
    }
    int Groups() const { return groups_; }

    static uint32_t DecodeAt(const std::string &s, size_t &i) {
        auto b = static_cast<unsigned char>(s[i]);
        int len = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : (b >> 3) == 30 ? 4 : 0;
        if (len == 0 || i + len > s.size()) {
            throw InvalidInputException("invalid UTF-8 in a regular expression or its subject");
        }
        uint32_t cp = len == 1 ? b : len == 2 ? (b & 0x1F) : len == 3 ? (b & 0x0F) : (b & 0x07);
        for (int k = 1; k < len; k++) {
            auto cb = static_cast<unsigned char>(s[i + k]);
            if ((cb >> 6) != 2) {
                throw InvalidInputException("invalid UTF-8 in a regular expression or its subject");
            }
            cp = (cp << 6) | (cb & 0x3F);
        }
        i += len;
        return cp;
    }

private:
    bool More() const { return pos_ < p_.size(); }
    char Peek() const { return p_[pos_]; }

    NodePtr ParseAlt(int depth) {
        if (depth > kMaxDepth) Unsupported("nested this deep");
        auto first = ParseConcat(depth);
        if (!More() || Peek() != '|') return first;
        auto alt = Make(NodeKind::Alt);
        alt->kids.push_back(std::move(first));
        while (More() && Peek() == '|') {
            pos_++;
            alt->kids.push_back(ParseConcat(depth));
        }
        return alt;
    }

    NodePtr ParseConcat(int depth) {
        auto cat = Make(NodeKind::Concat);
        while (More() && Peek() != '|' && Peek() != ')') {
            auto atom = ParseAtom(depth);
            atom = ParseQuantifiers(std::move(atom));
            cat->kids.push_back(std::move(atom));
        }
        return cat;
    }

    static bool IsQuant(char c) { return c == '*' || c == '+' || c == '?' || c == '{'; }

    NodePtr ParseQuantifiers(NodePtr atom) {
        if (!More() || !IsQuant(Peek())) return atom;
        int min = 0, max = -1;
        char q = Peek();
        if (q == '{') {
            ParseBraces(min, max);
        } else {
            pos_++;
            if (q == '*') { min = 0; max = -1; }
            else if (q == '+') { min = 1; max = -1; }
            else { min = 0; max = 1; }
        }
        bool lazy = false;
        if (More() && Peek() == '?') {
            lazy = true;
            pos_++;
        }
        if (More() && IsQuant(Peek())) Invalid("bad repetition operator in " + p_);
        if (atom->kind == NodeKind::Begin || atom->kind == NodeKind::End ||
            atom->kind == NodeKind::WordB || atom->kind == NodeKind::NotWordB || Nullable(*atom)) {
            // RE2 has its own rules for a repeat that can match empty.
            Unsupported("repeat of what can match empty, in " + p_);
        }
        auto rep = Make(NodeKind::Repeat);
        rep->min = min;
        rep->max = max;
        rep->lazy = lazy;
        rep->kids.push_back(std::move(atom));
        return rep;
    }

    // {n}, {n,}, {n,m}. RE2 reads any other { as a literal, which the
    // subset refuses.
    void ParseBraces(int &min, int &max) {
        size_t i = pos_ + 1;
        auto number = [&](int &out) {
            size_t s = i;
            long v = 0;
            while (i < p_.size() && p_[i] >= '0' && p_[i] <= '9' && i - s < 7) v = v * 10 + (p_[i++] - '0');
            if (i == s) return false;
            out = static_cast<int>(v);
            return true;
        };
        if (!number(min)) Unsupported("{ that is not a repeat, in " + p_);
        if (i < p_.size() && p_[i] == '}') {
            max = min;
        } else if (i < p_.size() && p_[i] == ',') {
            i++;
            if (i < p_.size() && p_[i] == '}') {
                max = -1;
            } else if (!number(max) || i >= p_.size() || p_[i] != '}') {
                Unsupported("{ that is not a repeat, in " + p_);
            }
        } else {
            Unsupported("{ that is not a repeat, in " + p_);
        }
        pos_ = i + 1;
        if (min > kMaxRepeat || max > kMaxRepeat || (max >= 0 && max < min)) {
            Invalid("bad repetition operator in " + p_);
        }
    }

    NodePtr ParseAtom(int depth) {
        char c = Peek();
        if (c == '*' || c == '+' || c == '?' || c == '{') {
            Invalid("missing argument to repetition operator in " + p_);
        }
        if (c == '(') {
            pos_++;
            int cap = -1;
            if (More() && Peek() == '?') {
                if (pos_ + 1 < p_.size() && p_[pos_ + 1] == ':') {
                    pos_ += 2;
                } else {
                    Unsupported("group (? other than (?:, in " + p_);
                }
            } else {
                cap = ++groups_;
            }
            auto inner = ParseAlt(depth + 1);
            if (!More() || Peek() != ')') Invalid("missing ) in " + p_);
            pos_++;
            auto g = Make(NodeKind::Group);
            g->cap = cap;
            g->kids.push_back(std::move(inner));
            return g;
        }
        if (c == '[') return ParseClass();
        if (c == '.') {
            pos_++;
            return Make(NodeKind::Any);
        }
        if (c == '^') {
            pos_++;
            return Make(NodeKind::Begin);
        }
        if (c == '$') {
            pos_++;
            return Make(NodeKind::End);
        }
        if (c == '\\') return ParseEscape(false, nullptr);
        if (static_cast<unsigned char>(c) >= 0x80) Unsupported("with a non-ASCII character, " + p_);
        pos_++;
        auto ch = Make(NodeKind::Char);
        ch->c = static_cast<unsigned char>(c);
        return ch;
    }

    // An escape at pos_. In a class (cls set) a class escape adds to it
    // and nullptr comes back; otherwise a node.
    NodePtr ParseEscape(bool in_class, std::vector<Range> *cls) {
        pos_++;
        if (!More()) Invalid("trailing \\ in " + p_);
        char e = p_[pos_++];
        auto lit = [&](uint32_t cp) {
            if (in_class) {
                cls->push_back({cp, cp});
                return NodePtr();
            }
            auto ch = Make(NodeKind::Char);
            ch->c = cp;
            return ch;
        };
        auto klass = [&](const std::vector<Range> &r, bool neg) {
            auto ranges = neg ? Negate(r) : r;
            if (in_class) {
                cls->insert(cls->end(), ranges.begin(), ranges.end());
                return NodePtr();
            }
            auto n = Make(NodeKind::Class);
            n->ranges = ranges;
            return n;
        };
        switch (e) {
        case 'd': return klass(kDigit, false);
        case 'D': return klass(kDigit, true);
        case 'w': return klass(kWord, false);
        case 'W': return klass(kWord, true);
        case 's': return klass(kSpace, false);
        case 'S': return klass(kSpace, true);
        case 'n': return lit('\n');
        case 't': return lit('\t');
        case 'r': return lit('\r');
        case 'f': return lit('\f');
        case 'v': return lit('\v');
        case 'a': return lit(7);
        case 'b':
        case 'B':
            if (in_class) Unsupported("\\b in a class, in " + p_);
            return Make(e == 'b' ? NodeKind::WordB : NodeKind::NotWordB);
        case 'x': case 'p': case 'P': case 'Q': case 'E': case 'A': case 'z': case 'C':
            Unsupported(std::string("escape \\") + e + ", in " + p_);
        default:
            break;
        }
        if (e >= '0' && e <= '9') Unsupported(std::string("escape \\") + e + ", in " + p_);
        auto u = static_cast<unsigned char>(e);
        if (u >= 0x80) Unsupported("with a non-ASCII character, " + p_);
        if ((e >= 'a' && e <= 'z') || (e >= 'A' && e <= 'Z')) {
            throw InvalidInputException(std::string("invalid escape sequence: \\") + e);
        }
        return lit(u);  // \ before punctuation is the character itself
    }

    NodePtr ParseClass() {
        pos_++;  // [
        bool neg = false;
        if (More() && Peek() == '^') {
            neg = true;
            pos_++;
        }
        if (More() && Peek() == ']') Unsupported("class starting with ], in " + p_);
        std::vector<Range> ranges;
        while (true) {
            if (!More()) Invalid("missing ] in " + p_);
            char c = Peek();
            if (c == ']') {
                pos_++;
                break;
            }
            if (c == '[') Unsupported("[ in a class, in " + p_);
            uint32_t lo;
            if (c == '\\') {
                size_t before = ranges.size();
                ParseEscape(true, &ranges);
                // A single character from an escape may start a range.
                if (ranges.size() != before + 1 || ranges.back().first != ranges.back().second) continue;
                if (!(More() && Peek() == '-' && pos_ + 1 < p_.size() && p_[pos_ + 1] != ']')) continue;
                lo = ranges.back().first;
                ranges.pop_back();
            } else {
                if (static_cast<unsigned char>(c) >= 0x80) Unsupported("with a non-ASCII character, " + p_);
                pos_++;
                lo = static_cast<unsigned char>(c);
                if (!(More() && Peek() == '-' && pos_ + 1 < p_.size() && p_[pos_ + 1] != ']')) {
                    ranges.push_back({lo, lo});
                    continue;
                }
            }
            pos_++;  // -
            uint32_t hi;
            char h = Peek();
            if (h == '\\') {
                std::vector<Range> one;
                ParseEscape(true, &one);
                if (one.size() != 1 || one[0].first != one[0].second) Invalid("bad character class range in " + p_);
                hi = one[0].first;
            } else {
                if (h == '[') Unsupported("[ in a class, in " + p_);
                if (static_cast<unsigned char>(h) >= 0x80) Unsupported("with a non-ASCII character, " + p_);
                pos_++;
                hi = static_cast<unsigned char>(h);
            }
            if (hi < lo) Invalid("bad character class range in " + p_);
            ranges.push_back({lo, hi});
        }
        auto n = Make(NodeKind::Class);
        n->ranges = neg ? Negate(ranges) : ranges;
        return n;
    }

    const std::string &p_;
    bool literal_;
    size_t pos_ = 0;
    int groups_ = 0;
};

// ASCII case folding for 'i': each letter matches both cases. RE2 also
// folds k to U+212A and s to U+017F, and non-ASCII letters; a subject
// with any non-ASCII character is refused under 'i' (Regex::Search).
void FoldRanges(std::vector<Range> &r) {
    std::vector<Range> add;
    for (auto &x : r) {
        uint32_t lo = std::max<uint32_t>(x.first, 'a'), hi = std::min<uint32_t>(x.second, 'z');
        if (lo <= hi) add.push_back({lo - 32, hi - 32});
        lo = std::max<uint32_t>(x.first, 'A');
        hi = std::min<uint32_t>(x.second, 'Z');
        if (lo <= hi) add.push_back({lo + 32, hi + 32});
    }
    r.insert(r.end(), add.begin(), add.end());
}

}  // namespace

enum class Op : uint8_t { Char, Any, Class, Split, Jmp, Save, Match, Begin, End, WordB, NotWordB };

struct Inst {
    Op op;
    uint32_t c = 0;
    int x = 0, y = 0;  // Split: x before y; Jmp: x; Save: slot in x
    int cls = -1;
};

struct Program {
    std::vector<Inst> code;
    std::vector<std::vector<Range>> classes;
    int ncap = 2;  // slots: 2 per group, group 0 included
    bool has_any = false;
};

namespace {

class Compiler {
public:
    Compiler(Program &prog, bool fold) : prog_(prog), fold_(fold) {}

    void Emit(const Node &n) {
        if (prog_.code.size() > kMaxProgram) Unsupported("this large");
        switch (n.kind) {
        case NodeKind::Empty:
            break;
        case NodeKind::Char: {
            uint32_t c = n.c;
            bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            if (fold_ && letter) {
                std::vector<Range> r = {{c, c}};
                FoldRanges(r);
                AddClass(r);
            } else {
                Push({Op::Char, c});
            }
            break;
        }
        case NodeKind::Any:
            prog_.has_any = true;
            Push({Op::Any});
            break;
        case NodeKind::Class: {
            auto r = n.ranges;
            if (fold_) FoldRanges(r);
            AddClass(r);
            break;
        }
        case NodeKind::Begin: Push({Op::Begin}); break;
        case NodeKind::End: Push({Op::End}); break;
        case NodeKind::WordB: Push({Op::WordB}); break;
        case NodeKind::NotWordB: Push({Op::NotWordB}); break;
        case NodeKind::Concat:
            for (auto &k : n.kids) Emit(*k);
            break;
        case NodeKind::Alt: {
            // split L1, next; L1: a; jmp end; next: split L2, ... ; last
            std::vector<int> jumps;
            for (size_t i = 0; i + 1 < n.kids.size(); i++) {
                int split = Push({Op::Split});
                prog_.code[split].x = Here();
                Emit(*n.kids[i]);
                jumps.push_back(Push({Op::Jmp}));
                prog_.code[split].y = Here();
            }
            Emit(*n.kids.back());
            for (int j : jumps) prog_.code[j].x = Here();
            break;
        }
        case NodeKind::Group: {
            if (n.cap >= 0) {
                Inst s{Op::Save};
                s.x = 2 * n.cap;
                Push(s);
                Emit(*n.kids[0]);
                Inst e{Op::Save};
                e.x = 2 * n.cap + 1;
                Push(e);
            } else {
                Emit(*n.kids[0]);
            }
            break;
        }
        case NodeKind::Repeat:
            EmitRepeat(n);
            break;
        }
    }

    int Here() const { return static_cast<int>(prog_.code.size()); }

private:
    int Push(Inst i) {
        prog_.code.push_back(i);
        return static_cast<int>(prog_.code.size()) - 1;
    }

    void AddClass(const std::vector<Range> &r) {
        prog_.classes.push_back(r);
        Inst i{Op::Class};
        i.cls = static_cast<int>(prog_.classes.size()) - 1;
        Push(i);
    }

    // A split preferring `first` (greedy: the loop body; lazy: past it).
    int Split(bool lazy) {
        (void)lazy;
        return Push({Op::Split});
    }

    void SetSplit(int at, int body, int out, bool lazy) {
        prog_.code[at].x = lazy ? out : body;
        prog_.code[at].y = lazy ? body : out;
    }

    void EmitStar(const Node &kid, bool lazy) {
        int split = Split(lazy);
        int body = Here();
        Emit(kid);
        int jmp = Push({Op::Jmp});
        prog_.code[jmp].x = split;
        SetSplit(split, body, Here(), lazy);
    }

    void EmitOptional(const Node &kid, bool lazy) {
        int split = Split(lazy);
        int body = Here();
        Emit(kid);
        SetSplit(split, body, Here(), lazy);
    }

    void EmitRepeat(const Node &n) {
        const Node &kid = *n.kids[0];
        for (int i = 0; i < n.min; i++) Emit(kid);
        if (n.max < 0) {
            if (n.min > 0) {
                // x+ as x then x*; the last copy loops.
                EmitStar(kid, n.lazy);
            } else {
                EmitStar(kid, n.lazy);
            }
            return;
        }
        // x{n,m}: m - n nested optionals, x(x(x)?)?)?
        int extra = n.max - n.min;
        std::vector<int> splits;
        for (int i = 0; i < extra; i++) {
            int split = Split(n.lazy);
            splits.push_back(split);
            prog_.code[split].x = Here();  // body, fixed below
            Emit(kid);
        }
        int out = Here();
        for (int s : splits) SetSplit(s, s + 1, out, n.lazy);
    }

    Program &prog_;
    bool fold_;
};

bool IsWord(uint32_t c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool InClass(const std::vector<Range> &r, uint32_t c) {
    for (auto &x : r) {
        if (c >= x.first && c <= x.second) return true;
    }
    return false;
}

}  // namespace

Options ParseOptions(const std::string &text, bool allow_global) {
    Options o;
    for (char c : text) {
        switch (c) {
        case 'c': o.case_insensitive = false; break;
        case 'i': o.case_insensitive = true; break;
        case 'l': o.literal = true; break;
        case 'm': case 'n': case 'p': o.dot_nl = 0; break;
        case 's': o.dot_nl = 1; break;
        case 'g':
            if (!allow_global) {
                throw InvalidInputException(
                    "Option 'g' (global replace) is only valid for regexp_replace");
            }
            o.global = true;
            break;
        case ' ': case '\t': case '\n': break;
        default:
            throw InvalidInputException(std::string("Unrecognized Regex option ") + c);
        }
    }
    return o;
}

Regex::Regex(const std::string &pattern, const Options &options)
    : prog_(std::make_unique<Program>()), options_(options) {
    Parser parser(pattern, options.literal);
    auto ast = parser.Parse();
    prog_->ncap = 2 * (parser.Groups() + 1);
    Compiler comp(*prog_, options.case_insensitive);
    Inst s0{Op::Save};
    s0.x = 0;
    prog_->code.push_back(s0);
    comp.Emit(*ast);
    Inst s1{Op::Save};
    s1.x = 1;
    prog_->code.push_back(s1);
    prog_->code.push_back(Inst{Op::Match});
}

Regex::~Regex() = default;
Regex::Regex(Regex &&) noexcept = default;
Regex &Regex::operator=(Regex &&) noexcept = default;

int Regex::GroupCount() const { return prog_->ncap / 2 - 1; }

namespace {

struct ThreadList {
    std::vector<int> pcs;
    std::vector<int64_t> caps;  // pcs.size() * ncap
    std::vector<uint32_t> mark;  // mark[pc] == gen when pc is on the list
    uint32_t gen = 1;
    void Clear() {
        pcs.clear();
        caps.clear();
        gen++;
    }
};

}  // namespace

bool Regex::Search(const std::string &subject, size_t start, bool anchored, bool whole,
                   std::vector<std::pair<int64_t, int64_t>> &groups) const {
    const auto &code = prog_->code;
    // Code points and the byte offset of each, with the end's.
    std::vector<uint32_t> cps;
    std::vector<size_t> offs;
    cps.reserve(subject.size());
    offs.reserve(subject.size() + 1);
    bool non_ascii = false, newline = false;
    for (size_t i = 0; i < subject.size();) {
        offs.push_back(i);
        uint32_t cp = Parser::DecodeAt(subject, i);
        non_ascii = non_ascii || cp >= 0x80;
        newline = newline || cp == '\n';
        cps.push_back(cp);
    }
    offs.push_back(subject.size());
    if (options_.case_insensitive && non_ascii) {
        Unsupported("option 'i' over non-ASCII text");
    }
    if (newline && options_.dot_nl == 0) Unsupported("option 'm', 'n' or 'p' over text with a newline");
    if (newline && options_.dot_nl < 0 && prog_->has_any) {
        Unsupported(". over text with a newline, without option 's'");
    }
    const bool dot_nl = options_.dot_nl == 1;
    const size_t n = cps.size();
    size_t i0 = std::lower_bound(offs.begin(), offs.end(), start) - offs.begin();
    if (i0 >= offs.size() || offs[i0] != start) return false;

    const int ncap = prog_->ncap;
    ThreadList clist, nlist;
    clist.mark.assign(code.size(), 0);
    nlist.mark.assign(code.size(), 0);
    std::vector<int64_t> scratch(ncap, -1);
    std::vector<int64_t> best;
    bool matched = false;

    struct Frame {
        int pc;
        int slot;
        int64_t old;
    };
    std::vector<Frame> stack;

    // Follows the empty moves from pc at position sp, adding the threads
    // that consume a character (or match) to list l, in priority order.
    auto add = [&](ThreadList &l, int pc0, std::vector<int64_t> &caps, size_t sp) {
        stack.clear();
        stack.push_back({pc0, -1, 0});
        while (!stack.empty()) {
            Frame f = stack.back();
            stack.pop_back();
            if (f.slot >= 0) {
                caps[f.slot] = f.old;
                continue;
            }
            int pc = f.pc;
            if (l.mark[pc] == l.gen) continue;
            l.mark[pc] = l.gen;
            const Inst &in = code[pc];
            switch (in.op) {
            case Op::Jmp:
                stack.push_back({in.x, -1, 0});
                break;
            case Op::Split:
                stack.push_back({in.y, -1, 0});
                stack.push_back({in.x, -1, 0});
                break;
            case Op::Save:
                stack.push_back({-1, in.x, caps[in.x]});
                caps[in.x] = static_cast<int64_t>(sp);
                stack.push_back({pc + 1, -1, 0});
                break;
            case Op::Begin:
                if (sp == 0) stack.push_back({pc + 1, -1, 0});
                break;
            case Op::End:
                if (sp == n) stack.push_back({pc + 1, -1, 0});
                break;
            case Op::WordB:
            case Op::NotWordB: {
                bool before = sp > 0 && IsWord(cps[sp - 1]);
                bool after = sp < n && IsWord(cps[sp]);
                if ((before != after) == (in.op == Op::WordB)) stack.push_back({pc + 1, -1, 0});
                break;
            }
            default:
                l.pcs.push_back(pc);
                l.caps.insert(l.caps.end(), caps.begin(), caps.end());
                break;
            }
        }
    };

    for (size_t i = i0;; i++) {
        if (!matched && (!anchored || i == i0)) {
            std::fill(scratch.begin(), scratch.end(), -1);
            add(clist, 0, scratch, i);
        }
        if (clist.pcs.empty()) break;
        std::vector<int64_t> tcaps(ncap);
        for (size_t t = 0; t < clist.pcs.size(); t++) {
            int pc = clist.pcs[t];
            const Inst &in = code[pc];
            std::copy(clist.caps.begin() + t * ncap, clist.caps.begin() + (t + 1) * ncap, tcaps.begin());
            bool step = false;
            switch (in.op) {
            case Op::Match:
                if (whole && i != n) break;
                best = tcaps;
                matched = true;
                t = clist.pcs.size();  // drop the threads below it
                continue;
            case Op::Char:
                step = i < n && cps[i] == in.c;
                break;
            case Op::Any:
                step = i < n && (dot_nl || cps[i] != '\n');
                break;
            case Op::Class:
                step = i < n && InClass(prog_->classes[in.cls], cps[i]);
                break;
            default:
                break;
            }
            if (step) add(nlist, pc + 1, tcaps, i + 1);
        }
        std::swap(clist, nlist);
        nlist.Clear();
        if (i >= n) break;
    }
    if (!matched) return false;
    groups.assign(ncap / 2, {-1, -1});
    for (int g = 0; g < ncap / 2; g++) {
        int64_t a = best[2 * g], b = best[2 * g + 1];
        if (a >= 0 && b >= 0) {
            groups[g] = {static_cast<int64_t>(offs[a]), static_cast<int64_t>(offs[b])};
        }
    }
    return true;
}

namespace {

void Rewrite(std::string &out, const std::string &rewrite, const std::string &subject,
             const std::vector<std::pair<int64_t, int64_t>> &groups) {
    for (size_t k = 0; k < rewrite.size(); k++) {
        char c = rewrite[k];
        if (c != '\\') {
            out += c;
            continue;
        }
        k++;
        if (k >= rewrite.size()) Invalid("rewrite ending in \\");
        char d = rewrite[k];
        if (d == '\\') {
            out += '\\';
        } else if (d >= '0' && d <= '9') {
            size_t g = static_cast<size_t>(d - '0');
            if (g >= groups.size()) Invalid("rewrite \\" + std::string(1, d) + " past the pattern's groups");
            if (groups[g].first >= 0) {
                out.append(subject, static_cast<size_t>(groups[g].first),
                           static_cast<size_t>(groups[g].second - groups[g].first));
            }
        } else {
            Invalid("invalid rewrite pattern \\" + std::string(1, d));
        }
    }
}

}  // namespace

std::string Replace(const Regex &re, const std::string &subject, const std::string &rewrite,
                    bool global) {
    // The rewrite is checked once, as RE2 checks it before replacing.
    {
        std::vector<std::pair<int64_t, int64_t>> none(re.GroupCount() + 1, {-1, -1});
        std::string dummy;
        Rewrite(dummy, rewrite, subject, none);
    }
    std::vector<std::pair<int64_t, int64_t>> g;
    if (!global) {
        if (!re.Search(subject, 0, false, false, g)) return subject;
        std::string out = subject.substr(0, static_cast<size_t>(g[0].first));
        Rewrite(out, rewrite, subject, g);
        out.append(subject, static_cast<size_t>(g[0].second), std::string::npos);
        return out;
    }
    // RE2::GlobalReplace: an empty match where the last one ended is
    // skipped, one code point at a time.
    std::string out;
    size_t p = 0;
    int64_t lastend = -1;
    size_t count = 0;
    while (p <= subject.size()) {
        if (!re.Search(subject, p, false, false, g)) break;
        auto ms = static_cast<size_t>(g[0].first), me = static_cast<size_t>(g[0].second);
        if (p < ms) out.append(subject, p, ms - p);
        if (static_cast<int64_t>(ms) == lastend && ms == me) {
            if (p < subject.size()) {
                size_t q = p;
                Parser::DecodeAt(subject, q);
                out.append(subject, p, q - p);
                p = q;
            } else {
                p++;
            }
            continue;
        }
        Rewrite(out, rewrite, subject, g);
        p = me;
        lastend = static_cast<int64_t>(me);
        count++;
    }
    if (count == 0) return subject;
    if (p < subject.size()) out.append(subject, p, std::string::npos);
    return out;
}

}  // namespace regex
}  // namespace zugodb
