#pragma once

// The regular expressions of DuckDB's regexp_ functions (libzugodb Stage
// 8, #32; design L24). DuckDB uses RE2; this is a subset of RE2's syntax
// matched as RE2 matches it: leftmost-first over code points, by a Pike
// VM (a Thompson NFA with captures), so in time linear in the subject and
// with no recursion. What the subset does not hold is refused when the
// pattern is compiled: an error, never a match RE2 would not make.
//
// The subset: literal characters; \ before punctuation; \d \D \w \W \s
// \S \b \B \n \t \r \f \v; . ; bracket classes of characters, ranges and
// those escapes, negated with ^; ( ) and (?: ); * + ? {n} {n,} {n,m}
// with up to 1000, each also lazy with ?; | ; ^ and $ at the text's
// start and end. Refused: any other (? group, backreferences, \x \p \Q
// \A \z \C, a { that is not a repeat, a repeat of what can match empty,
// POSIX classes, a class starting with ], a non-ASCII pattern character
// outside literal mode.

#include "slothdb/common/constants.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace zugodb {
namespace regex {

// DuckDB's option letters (ParseRegexOptions at the pinned tag).
struct Options {
    bool case_insensitive = false;
    bool literal = false;
    // -1: neither 's' nor 'm'/'n'/'p'; 1: 's', . matches \n; 0: 'm', 'n'
    // or 'p'. Where it matters (a subject holding \n), only 's' is read.
    int dot_nl = -1;
    bool global = false;
};

// Throws InvalidInputException for a letter DuckDB refuses, and for 'g'
// unless allow_global. Spaces and tabs are skipped, as DuckDB does.
Options ParseOptions(const std::string &text, bool allow_global);

struct Program;

class Regex {
public:
    // Throws InvalidInputException for what RE2 refuses too, and
    // NotImplementedException for what the subset does not hold.
    Regex(const std::string &pattern, const Options &options);
    ~Regex();
    Regex(Regex &&) noexcept;
    Regex &operator=(Regex &&) noexcept;

    // The capturing groups, not counting group 0.
    int GroupCount() const;

    // Searches subject from byte offset `start` for the leftmost-first
    // match; anchored, only at `start`, and whole, only one ending at the
    // subject's end. On a match, groups[k] is the byte span of group k
    // (k = 0 the whole match), {-1, -1} for a group that took no part.
    // Throws for a subject that is not UTF-8, and where the subset cannot
    // say what RE2 would (a subject holding \n against . with no 's').
    bool Search(const std::string &subject, size_t start, bool anchored, bool whole,
                std::vector<std::pair<int64_t, int64_t>> &groups) const;

private:
    std::unique_ptr<Program> prog_;
    Options options_;
};

// RE2's Replace and GlobalReplace with DuckDB's options: \0 to \9 for a
// group, \\ for a backslash; any other escape, or a group past the
// pattern's, is an error.
std::string Replace(const Regex &re, const std::string &subject, const std::string &rewrite,
                    bool global);

} // namespace regex
} // namespace zugodb
