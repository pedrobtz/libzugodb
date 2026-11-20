// DuckDB's text for the engine's values, and the comparison of one value
// with the recorded one (design L15).
//
// DuckDB's runner turns each value into text with its cast to VARCHAR
// (result_helper.cpp, SQLLogicTestConvertValue): NULL is "NULL", a
// boolean "1" or "0", the empty string "(empty)", a NUL byte "\0". The
// casts below follow DuckDB's at the pinned tag: doubles through its
// vendored fmt 6.1.2 "{}" (string_cast.cpp), dates and times through
// cast_helpers.hpp, blobs through blob.cpp.

#include "slt.hpp"

#include "slothdb/common/types/value.hpp"

#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace zugodb::slt {

namespace {

// fmt 6's "{}" for a shortest digit string: `digits` holds the
// significant digits, the value is 0.d1d2... * 10^(exp10 + 1), i.e.
// d1.d2... * 10^exp10.
std::string Prettify(bool negative, const std::string &digits, int exp10) {
    std::string out = negative ? "-" : "";
    int n = static_cast<int>(digits.size());
    if (!(exp10 >= -4 && exp10 < 16)) {
        out += digits[0];
        if (n > 1) out += "." + digits.substr(1);
        out += 'e';
        int e = exp10;
        out += e < 0 ? '-' : '+';
        if (e < 0) e = -e;
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02d", e);
        out += buf;
        return out;
    }
    int full_exp = exp10 + 1;  // digits before the decimal point
    if (n <= full_exp) {
        out += digits + std::string(static_cast<size_t>(full_exp - n), '0') + ".0";
    } else if (full_exp > 0) {
        out += digits.substr(0, static_cast<size_t>(full_exp)) + "." + digits.substr(static_cast<size_t>(full_exp));
    } else {
        out += "0." + std::string(static_cast<size_t>(-full_exp), '0') + digits;
    }
    return out;
}

template <typename T>
std::string FormatShortest(T value) {
    if (std::isnan(value)) return std::signbit(value) ? "-nan" : "nan";
    if (std::isinf(value)) return value < 0 ? "-inf" : "inf";
    bool negative = std::signbit(value);
    if (value == 0) return negative ? "-0.0" : "0.0";
    T mag = negative ? -value : value;
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof(buf), mag, std::chars_format::scientific);
    std::string sci(buf, res.ptr);
    // d[.ddd]e[+-]XX
    size_t epos = sci.find('e');
    std::string mant = sci.substr(0, epos);
    int exp10 = std::atoi(sci.c_str() + epos + 1);
    std::string digits;
    for (char ch : mant)
        if (ch != '.') digits += ch;
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    return Prettify(negative, digits, exp10);
}

void DaysToCivil(int64_t days, int64_t &y, unsigned &m, unsigned &d) {
    int64_t z = days + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = static_cast<unsigned>(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t yr = static_cast<int64_t>(yoe) + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y = yr + (m <= 2);
}

// cast_helpers.hpp DateToStringCast: YYYY-MM-DD, more year digits past
// 9999, and year 0 and before as "(BC)" with year 1 - y.
std::string FormatDays(int64_t days) {
    int64_t y;
    unsigned m, d;
    DaysToCivil(days, y, m, d);
    bool bc = y <= 0;
    if (bc) y = -y + 1;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02u%s", static_cast<long long>(y), m, d, bc ? " (BC)" : "");
    return buf;
}

// TimeToStringCast: HH:MM:SS, then .micros with the trailing zeros cut.
std::string FormatTimeOfDay(int64_t micros_of_day) {
    int64_t secs = micros_of_day / 1000000;
    int64_t us = micros_of_day % 1000000;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", static_cast<int>(secs / 3600),
                  static_cast<int>((secs / 60) % 60), static_cast<int>(secs % 60));
    std::string out = buf;
    if (us != 0) {
        std::snprintf(buf, sizeof(buf), "%06d", static_cast<int>(us));
        std::string frac = buf;
        while (!frac.empty() && frac.back() == '0') frac.pop_back();
        out += "." + frac;
    }
    return out;
}

std::string FormatTimestampMicros(int64_t micros) {
    int64_t days = micros / 86400000000LL;
    int64_t rem = micros % 86400000000LL;
    if (rem < 0) {
        rem += 86400000000LL;
        days -= 1;
    }
    // DuckDB keeps "(BC)" after the date, before the time
    return FormatDays(days) + " " + FormatTimeOfDay(rem);
}

std::string FormatBlob(const std::string &bytes) {
    static const char *hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char ch : bytes) {
        if (ch >= 32 && ch <= 126 && ch != '\\' && ch != '\'' && ch != '"') {
            out += static_cast<char>(ch);
        } else {
            out += "\\x";
            out += hex[ch >> 4];
            out += hex[ch & 0x0F];
        }
    }
    return out;
}

std::string Text(std::string s) {
    if (s.empty()) return "(empty)";
    std::string out;
    for (char ch : s) {
        if (ch == '\0') out += "\\0";
        else out += ch;
    }
    return out;
}

std::string Lower(std::string s) {
    for (auto &ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool ParseNumber(const std::string &s, double &out) {
    if (s.empty()) return false;
    std::string l = Lower(s);
    if (l == "nan" || l == "-nan") {
        out = std::nan("");
        return true;
    }
    if (l == "inf" || l == "infinity") {
        out = INFINITY;
        return true;
    }
    if (l == "-inf" || l == "-infinity") {
        out = -INFINITY;
        return true;
    }
    char *end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end && *end == '\0' && end != s.c_str();
}

std::string Trim(const std::string &s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

// Text DuckDB's cast could have made from a FLOAT or DOUBLE (above), or a
// rounded value written as one: digits with a fractional part that does
// not end in 0 (but ".0" alone), an exponent, inf or nan.
bool IsDoubleShaped(const std::string &s) {
    std::string l = Lower(s);
    size_t i = 0;
    if (i < l.size() && l[i] == '-') i++;
    std::string rest = l.substr(i);
    if (rest == "inf" || rest == "nan" || rest == "infinity") return true;
    size_t int_digits = 0;
    while (i < l.size() && std::isdigit(static_cast<unsigned char>(l[i]))) i++, int_digits++;
    if (int_digits == 0) return false;
    if (i < l.size() && l[i] == '.') {
        i++;
        size_t frac_start = i;
        while (i < l.size() && std::isdigit(static_cast<unsigned char>(l[i]))) i++;
        std::string frac = l.substr(frac_start, i - frac_start);
        if (frac.empty()) return false;
        if (frac.back() == '0' && frac != "0") return false;
        if (i == l.size()) return true;
    } else if (i == l.size()) {
        return false;  // an integer
    }
    if (l[i] != 'e') return false;
    i++;
    if (i < l.size() && (l[i] == '+' || l[i] == '-')) i++;
    size_t exp_digits = 0;
    while (i < l.size() && std::isdigit(static_cast<unsigned char>(l[i]))) i++, exp_digits++;
    return exp_digits > 0 && i == l.size();
}

} // namespace

std::string FormatDouble(double value) { return FormatShortest(value); }
std::string FormatFloat(float value) { return FormatShortest(value); }

std::string FormatValue(const Value &v) {
    if (v.IsNull()) return "NULL";
    switch (v.type().id()) {
    case LogicalTypeId::BOOLEAN:
        return v.GetValue<bool>() ? "1" : "0";
    case LogicalTypeId::TINYINT:
        return std::to_string(v.GetValue<int8_t>());
    case LogicalTypeId::SMALLINT:
        return std::to_string(v.GetValue<int16_t>());
    case LogicalTypeId::INTEGER:
        return std::to_string(v.GetValue<int32_t>());
    case LogicalTypeId::BIGINT:
        return std::to_string(v.GetValue<int64_t>());
    case LogicalTypeId::UTINYINT:
        return std::to_string(v.GetValue<uint8_t>());
    case LogicalTypeId::USMALLINT:
        return std::to_string(v.GetValue<uint16_t>());
    case LogicalTypeId::UINTEGER:
        return std::to_string(v.GetValue<uint32_t>());
    case LogicalTypeId::UBIGINT:
        return std::to_string(v.GetValue<uint64_t>());
    case LogicalTypeId::HUGEINT:
        return v.ToString();
    case LogicalTypeId::FLOAT:
        return FormatFloat(v.GetValue<float>());
    case LogicalTypeId::DOUBLE:
        return FormatDouble(v.GetValue<double>());
    case LogicalTypeId::VARCHAR:
        return Text(v.GetValue<std::string>());
    case LogicalTypeId::BLOB:
        return Text(FormatBlob(v.GetValue<std::string>()));
    case LogicalTypeId::DATE:
        return FormatDays(v.GetValue<int32_t>());
    case LogicalTypeId::TIME: {
        int64_t us = v.GetValue<int64_t>() % 86400000000LL;
        if (us < 0) us += 86400000000LL;
        return FormatTimeOfDay(us);
    }
    case LogicalTypeId::TIMESTAMP:
        return FormatTimestampMicros(v.GetValue<int64_t>());
    case LogicalTypeId::TIMESTAMP_TZ:
        // DuckDB shows the offset of its TimeZone setting, UTC in the tests
        return FormatTimestampMicros(v.GetValue<int64_t>()) + "+00";
    default:
        // A type DuckDB would print differently (LIST, STRUCT, ...): the
        // engine's own text, which then fails to match.
        return Text(v.ToString());
    }
}

bool ValueMatches(const std::string &actual, const std::string &expected_text, ColumnKind kind, char declared) {
    if (actual == expected_text) return true;
    if (actual == "NULL" || expected_text == "NULL") return false;
    switch (kind) {
    case ColumnKind::Bool: {
        auto as_bool = [](const std::string &s, int &out) {
            std::string l = Lower(s);
            if (l == "true" || s == "1") return out = 1, true;
            if (l == "false" || s == "0") return out = 0, true;
            return false;
        };
        int a, e;
        return as_bool(actual, a) && as_bool(expected_text, e) && a == e;
    }
    case ColumnKind::Integer: {
        // DuckDB's cast of the recorded text to the column's type trims
        // spaces and drops a fractional part of zeros ("85.000000"). Its
        // own text for a DOUBLE ("85.0") is not taken for an integer.
        std::string e = Trim(expected_text);
        if (e == actual) return true;
        size_t dot = e.find('.');
        if (dot == std::string::npos || IsDoubleShaped(e)) return false;
        std::string frac = e.substr(dot + 1);
        if (frac.empty() || frac.find_first_not_of('0') != std::string::npos) return false;
        return e.substr(0, dot) == actual;
    }
    case ColumnKind::Float: {
        // DuckDB's ApproxEqual (types.cpp) on both texts read as numbers.
        double a, x;
        if (!ParseNumber(actual, a) || !ParseNumber(Trim(expected_text), x)) return false;
        if (std::isnan(a) && std::isnan(x)) return true;
        if (!std::isfinite(a) || !std::isfinite(x)) return a == x;
        double epsilon = std::fabs(a) * 0.01 + 0.00000001;
        return std::fabs(x - a) <= epsilon;
    }
    case ColumnKind::Other:
        break;
    }
    return false;
}

} // namespace zugodb::slt
