#pragma once

// Stub providers for the doctests (Stage 11.1, #40): R-free and
// deterministic, written against providers.hpp alone. They test the
// engine's plumbing, not a dialect: the regex stub is a literal matcher.
//
// The regex stub's patterns:
//   - a literal, matched byte for byte (ASCII case folded under
//     ZUGODB_REGEX_CASELESS), in which each "(...)" pair is a capture group
//     around a literal piece and is not matched itself; groups do not nest;
//   - "!<member>:<status>", which compiles, and then <member> (compile,
//     group_count, match_create, search, group or replace) returns
//     <status>, a decimal integer, every time;
//   - "!grow:<literal>", whose replace() first says the buffer is too
//     small (asking for twice the capacity it was given), then replaces;
//   - "!shrink:<literal>", whose replace() always says it is too small,
//     asking for no more than it was given.
// Its replace() reads \0 to \9 and \\ in the rewrite; any other backslash
// is ZUGODB_PROVIDER_ERR_BAD_REWRITE. Its match state keeps the last
// offsets and a scratch counter, written on every search, so that TSan
// would see one match state shared by threads.
//
// The UTF-8 stub validates by the structure of UTF-8's lead and
// continuation bytes, counts code points, and fails with
// ZUGODB_PROVIDER_ERR_OTHER on text that holds the byte 0x01.

#include "slothdb/main/providers.hpp"

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace stub {

struct Counters {
    std::atomic<long> compiles{0};
    std::atomic<long> patterns_live{0};
    std::atomic<long> matches_created{0};
    std::atomic<long> matches_live{0};
    std::atomic<long> searches{0};
    std::atomic<long> utf8_counts{0};
    void Reset() {
        compiles = 0;
        patterns_live = 0;
        matches_created = 0;
        matches_live = 0;
        searches = 0;
        utf8_counts = 0;
    }
};
inline Counters &Count() {
    static Counters c;
    return c;
}

} // namespace stub

struct zugodb_regex_pattern {
    std::string literal;                   // the text to find, parentheses removed
    std::vector<std::pair<size_t, size_t>> groups; // offsets into literal
    bool caseless = false;
    std::string fail_member;               // "" or the member that fails
    int fail_status = 0;
    int grow = 0;                          // 1: !grow, 2: !shrink
};

struct zugodb_regex_match {
    const zugodb_regex_pattern *p = nullptr;
    bool matched = false;
    size_t start = 0;
    unsigned long scratch = 0;
    bool grown = false;
};

namespace stub {

inline bool Fails(const zugodb_regex_pattern *p, const char *member) {
    return p && p->fail_member == member;
}

inline bool EqualAt(const zugodb_regex_pattern &p, const char *text, size_t len, size_t at) {
    if (at + p.literal.size() > len) return false;
    for (size_t k = 0; k < p.literal.size(); k++) {
        unsigned char a = static_cast<unsigned char>(text[at + k]);
        unsigned char b = static_cast<unsigned char>(p.literal[k]);
        if (p.caseless) {
            a = static_cast<unsigned char>(std::tolower(a));
            b = static_cast<unsigned char>(std::tolower(b));
        }
        if (a != b) return false;
    }
    return true;
}

// The first match at or after `from`, or len + 1.
inline size_t Find(const zugodb_regex_pattern &p, const char *text, size_t len, size_t from) {
    for (size_t at = from; at + p.literal.size() <= len; at++) {
        if (EqualAt(p, text, len, at)) return at;
    }
    return len + 1;
}

inline int Compile(const char *pattern, size_t len, uint32_t flags, zugodb_regex_pattern **out,
                   char *errbuf, size_t errlen) {
    Count().compiles++;
    if (!out) return ZUGODB_PROVIDER_ERR_ARGUMENT;
    *out = nullptr;
    if (flags & ~ZUGODB_REGEX_CASELESS) return ZUGODB_PROVIDER_ERR_ARGUMENT;
    std::string s(pattern, len);
    auto *p = new zugodb_regex_pattern();
    p->caseless = (flags & ZUGODB_REGEX_CASELESS) != 0;
    if (s.rfind("!grow:", 0) == 0) {
        p->grow = 1;
        s = s.substr(6);
    } else if (s.rfind("!shrink:", 0) == 0) {
        p->grow = 2;
        s = s.substr(8);
    } else if (!s.empty() && s[0] == '!') {
        auto colon = s.find(':');
        if (colon == std::string::npos) {
            delete p;
            return ZUGODB_PROVIDER_ERR_BAD_PATTERN;
        }
        p->fail_member = s.substr(1, colon - 1);
        p->fail_status = std::atoi(s.c_str() + colon + 1);
        s.clear();
        if (p->fail_member == "compile") {
            int st = p->fail_status;
            delete p;
            if (errbuf && errlen) {
                std::snprintf(errbuf, errlen, "the stub refuses this pattern");
            }
            return st;
        }
    }
    bool open = false;
    size_t gstart = 0;
    for (char c : s) {
        if (c == '(') {
            if (open) {
                delete p;
                if (errbuf && errlen) std::snprintf(errbuf, errlen, "nested group");
                return ZUGODB_PROVIDER_ERR_BAD_PATTERN;
            }
            open = true;
            gstart = p->literal.size();
        } else if (c == ')') {
            if (!open) {
                delete p;
                return ZUGODB_PROVIDER_ERR_BAD_PATTERN;
            }
            open = false;
            p->groups.emplace_back(gstart, p->literal.size());
        } else {
            p->literal += c;
        }
    }
    if (open) {
        delete p;
        return ZUGODB_PROVIDER_ERR_BAD_PATTERN;
    }
    Count().patterns_live++;
    *out = p;
    return ZUGODB_PROVIDER_OK;
}

inline void PatternFree(zugodb_regex_pattern *p) {
    if (!p) return;
    Count().patterns_live--;
    delete p;
}

inline int GroupCount(const zugodb_regex_pattern *p) {
    if (Fails(p, "group_count")) return p->fail_status;
    return static_cast<int>(p->groups.size());
}

inline int MatchCreate(const zugodb_regex_pattern *p, zugodb_regex_match **out) {
    if (Fails(p, "match_create")) return p->fail_status;
    auto *m = new zugodb_regex_match();
    m->p = p;
    Count().matches_created++;
    Count().matches_live++;
    *out = m;
    return ZUGODB_PROVIDER_OK;
}

inline void MatchFree(zugodb_regex_match *m) {
    if (!m) return;
    Count().matches_live--;
    delete m;
}

inline int Search(const zugodb_regex_pattern *p, const char *text, size_t len, uint32_t options,
                  zugodb_regex_match *m) {
    Count().searches++;
    if (!p || !m || m->p != p) return ZUGODB_PROVIDER_ERR_ARGUMENT;
    if (options & ~ZUGODB_REGEX_ANCHOR_BOTH) return ZUGODB_PROVIDER_ERR_ARGUMENT;
    m->scratch++;
    m->matched = false;
    if (Fails(p, "search")) return p->fail_status;
    if (options & ZUGODB_REGEX_ANCHOR_BOTH) {
        if (len != p->literal.size() || !EqualAt(*p, text, len, 0)) return ZUGODB_PROVIDER_NOMATCH;
        m->matched = true;
        m->start = 0;
        return ZUGODB_PROVIDER_OK;
    }
    size_t at = Find(*p, text, len, 0);
    if (at > len) return ZUGODB_PROVIDER_NOMATCH;
    m->matched = true;
    m->start = at;
    return ZUGODB_PROVIDER_OK;
}

inline int Group(const zugodb_regex_match *m, int g, size_t *start, size_t *end) {
    if (!m || !m->matched || !start || !end) return ZUGODB_PROVIDER_ERR_ARGUMENT;
    if (Fails(m->p, "group")) return m->p->fail_status;
    if (g == 0) {
        *start = m->start;
        *end = m->start + m->p->literal.size();
        return ZUGODB_PROVIDER_OK;
    }
    if (g < 0 || static_cast<size_t>(g) > m->p->groups.size()) return ZUGODB_PROVIDER_ERR_ARGUMENT;
    *start = m->start + m->p->groups[g - 1].first;
    *end = m->start + m->p->groups[g - 1].second;
    return ZUGODB_PROVIDER_OK;
}

inline int Replace(const zugodb_regex_pattern *p, const char *text, size_t len,
                   const char *rewrite, size_t rlen, uint32_t options, zugodb_regex_match *m,
                   char *out, size_t *outlen) {
    if (!p || !m || !outlen) return ZUGODB_PROVIDER_ERR_ARGUMENT;
    if (options & ~ZUGODB_REGEX_REPLACE_GLOBAL) return ZUGODB_PROVIDER_ERR_ARGUMENT;
    if (Fails(p, "replace")) return p->fail_status;
    if (p->grow == 2) return ZUGODB_PROVIDER_ERR_BUFFER_TOO_SMALL; // *outlen unchanged
    if (p->literal.empty()) return ZUGODB_PROVIDER_ERR_ARGUMENT;
    std::string res;
    size_t from = 0;
    bool global = (options & ZUGODB_REGEX_REPLACE_GLOBAL) != 0;
    while (true) {
        size_t at = Find(*p, text, len, from);
        if (at > len) break;
        res.append(text + from, at - from);
        for (size_t k = 0; k < rlen; k++) {
            if (rewrite[k] != '\\') {
                res += rewrite[k];
                continue;
            }
            if (k + 1 >= rlen) return ZUGODB_PROVIDER_ERR_BAD_REWRITE;
            char c = rewrite[++k];
            if (c == '\\') {
                res += '\\';
            } else if (c >= '0' && c <= '9') {
                int g = c - '0';
                if (g == 0) {
                    res.append(text + at, p->literal.size());
                } else if (static_cast<size_t>(g) <= p->groups.size()) {
                    auto gr = p->groups[g - 1];
                    res.append(text + at + gr.first, gr.second - gr.first);
                } else {
                    return ZUGODB_PROVIDER_ERR_BAD_REWRITE;
                }
            } else {
                return ZUGODB_PROVIDER_ERR_BAD_REWRITE;
            }
        }
        from = at + p->literal.size();
        if (!global) break;
    }
    res.append(text + from, len - from);
    if (p->grow == 1 && !m->grown) {
        m->grown = true;
        *outlen = *outlen * 2 + 1;
        return ZUGODB_PROVIDER_ERR_BUFFER_TOO_SMALL;
    }
    if (res.size() > *outlen) {
        *outlen = res.size();
        return ZUGODB_PROVIDER_ERR_BUFFER_TOO_SMALL;
    }
    if (!res.empty()) std::memcpy(out, res.data(), res.size());
    *outlen = res.size();
    return ZUGODB_PROVIDER_OK;
}

inline const char *StrError(int status) {
    switch (status) {
    case ZUGODB_PROVIDER_ERR_LIMIT: return "stub: match limit";
    case ZUGODB_PROVIDER_ERR_INVALID_UTF8: return "stub: invalid UTF-8";
    case ZUGODB_PROVIDER_ERR_BAD_PATTERN: return "stub: bad pattern";
    case ZUGODB_PROVIDER_ERR_BAD_REWRITE: return "stub: bad rewrite";
    case ZUGODB_PROVIDER_ERR_BUFFER_TOO_SMALL: return "stub: buffer too small";
    case ZUGODB_PROVIDER_ERR_NOMEM: return "stub: out of memory";
    case ZUGODB_PROVIDER_ERR_ARGUMENT: return "stub: bad argument";
    case ZUGODB_PROVIDER_ERR_OTHER: return "stub: other";
    default: return nullptr;
    }
}

inline zugodb_regex_v1 RegexTable() {
    zugodb_regex_v1 t{};
    t.abi_version = ZUGODB_REGEX_ABI_VERSION;
    t.struct_size = sizeof(zugodb_regex_v1);
    t.compile = Compile;
    t.pattern_free = PatternFree;
    t.group_count = GroupCount;
    t.match_create = MatchCreate;
    t.match_free = MatchFree;
    t.search = Search;
    t.group = Group;
    t.replace = Replace;
    t.strerror = StrError;
    return t;
}

// ---- UTF-8 ----------------------------------------------------------------

inline int Utf8Validate(const char *s, size_t n) {
    for (size_t i = 0; i < n;) {
        unsigned char b = static_cast<unsigned char>(s[i]);
        if (b == 0x01) return ZUGODB_PROVIDER_ERR_OTHER;
        size_t len = b < 0x80 ? 1 : (b >> 5) == 0x6 ? 2 : (b >> 4) == 0xE ? 3 : (b >> 3) == 0x1E ? 4 : 0;
        if (len == 0 || i + len > n) return ZUGODB_PROVIDER_ERR_INVALID_UTF8;
        for (size_t k = 1; k < len; k++) {
            if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) {
                return ZUGODB_PROVIDER_ERR_INVALID_UTF8;
            }
        }
        i += len;
    }
    return ZUGODB_PROVIDER_OK;
}

inline int Utf8Count(const char *s, size_t n, size_t *out) {
    Count().utf8_counts++;
    size_t c = 0;
    for (size_t i = 0; i < n; i++) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) c++;
    }
    *out = c;
    return ZUGODB_PROVIDER_OK;
}

inline const char *Utf8StrError(int status) {
    return status == ZUGODB_PROVIDER_ERR_OTHER ? "stub: utf8 failure" : nullptr;
}

inline zugodb_utf8_v1 Utf8Table() {
    zugodb_utf8_v1 t{};
    t.abi_version = ZUGODB_UTF8_ABI_VERSION;
    t.struct_size = sizeof(zugodb_utf8_v1);
    t.validate = Utf8Validate;
    t.count = Utf8Count;
    t.strerror = Utf8StrError;
    return t;
}

inline zugodb_providers Set(const zugodb_regex_v1 *regex, const zugodb_utf8_v1 *utf8) {
    zugodb_providers p{};
    p.abi_version = ZUGODB_PROVIDERS_ABI_VERSION;
    p.struct_size = sizeof(zugodb_providers);
    p.regex = regex;
    p.utf8 = utf8;
    return p;
}

} // namespace stub
