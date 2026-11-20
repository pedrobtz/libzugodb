// String functions of the pinned DuckDB subset that gave a wrong answer or
// accepted what DuckDB refuses (libzugodb Stage 8, #12; design L22). Each
// test runs the smallest SQL of the case's shape and expects DuckDB's
// recorded result from the corpus file it names (test/conformance/corpus/
// test/sql/...), or an error where the engine refuses the statement.

#include "doctest.h"
#include "../conformance_rows.hpp"
#include "slothdb/main/database.hpp"

using namespace zugodb;
using namespace zugodb::conformance;

// ---------------------------------------------------------------------------
// SUBSTRING, LEFT, RIGHT, INSTR, TRANSLATE and LIKE's '_' counted bytes,
// not characters (function/string/test_substring_utf8.test,
// test_substring.test, test_left.test, test_right.test,
// test_instr_utf8.test, test_translate.test, like_unicode.test,
// types/string/test_unicode.test), and SUBSTRING's negative offsets were
// not DuckDB's.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: SUBSTRING counts characters, with DuckDB's offsets") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT substring('u🦆', -2, 1), substring('A3🦤u🦆f', -3, 3), "
                    "substring('🦤🦆f', -3, 2), substring('twoñthree', 1, 4)") ==
          Rows{{"u", "u🦆f", "🦤🦆", "twoñ"}});
    CHECK(Run(conn, "SELECT substring('hello' from -2 for 2), substring('hello', 2, -2), "
                    "substring('hello', -2, -2), substring('abcd', -1), substring('abcd', -7), "
                    "substring('hello', 0, 2)") ==
          Rows{{"lo", "h", "el", "d", "abcd", "h"}});
    CHECK(Run(conn, "SELECT substring('hello', 2147483647, -2147483648), "
                    "substring('hello', -2147483647, 2147483647), substring('hello', -12345678, 12), "
                    "substring('hello', -2147483648, 2), substring('hello', 2147483647, 2147483647)") ==
          Rows{{"hello", "hello", "", "", ""}});
    CHECK_THROWS(conn.Query("SELECT substring('hello', 9223372036854775807, 1)"));
}

TEST_CASE("Stage 8: LEFT, RIGHT, INSTR, TRANSLATE, LIKE '_' and STRLEN by DuckDB's units") {
    Database db;
    Connection conn(db);
    CHECK(Run(conn, "SELECT LEFT('🦆ab', 1), LEFT('🦆ab', -1), LEFT('🦆ab', -4), RIGHT('🦆ab', 3), "
                    "RIGHT('🦆ab', -1), RIGHT('🦆ab', -4)") ==
          Rows{{"🦆", "🦆a", "", "🦆ab", "ab", ""}});
    CHECK(Run(conn, "SELECT instr('₡ four 🦆 end', '🦆 end'), strpos('🦆ab', 'b')") == Rows{{"8", "3"}});
    CHECK(Run(conn, "SELECT TRANSLATE('MotörHeadΩ', 'aeΩ', 'eÄ'), TRANSLATE('He🦆lo', 'lo', '🦆')") ==
          Rows{{"MotörHÄed", "He🦆🦆"}});
    CHECK(Run(conn, "SELECT '🦆a🦆' LIKE '_a_', '🦆🦆' LIKE '__', 'ö' LIKE '%_'") ==
          Rows{{"true", "true", "true"}});
    // STRLEN is bytes in DuckDB, LENGTH characters.
    CHECK(Run(conn, "SELECT strlen('🦆a'), length('🦆a'), length('twoñ')") == Rows{{"5", "2", "4"}});
    // REPEAT's count is an integer (test_repeat.test).
    CHECK_THROWS(conn.Query("SELECT REPEAT('hello', 'world')"));
}

// ---------------------------------------------------------------------------
// Case mapping and grapheme clusters (function/string/test_caseconvert.test,
// test_ilike.test, test_complex_unicode.test, test_reverse.test). DuckDB
// maps case with utf8proc's tables and reverses and counts grapheme
// clusters; the engine mapped ASCII only and reversed bytes. Where that
// could differ, refused.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: case mapping and grapheme clusters the engine cannot do are refused") {
    Database db;
    Connection conn(db);
    const char *queries[] = {
        "SELECT UPPER('ö')",
        "SELECT LOWER('MotörHead')",
        "SELECT 'MÜHLEISEN' ILIKE 'mühleisen'",
        "SELECT 'İ' ILIKE 'i'",
        "SELECT REVERSE('S\xCC\x88" "a')",       // S + COMBINING DIAERESIS
        "SELECT LENGTH('S\xCC\x88" "a')",
        "SELECT REVERSE('\xF0\x9F\xA4\xA6\xF0\x9F\x8F\xBC')",  // face palm + skin tone
    };
    for (auto *q : queries) {
        CAPTURE(q);
        CHECK_THROWS(conn.Query(q));
    }
    CHECK(Run(conn, "SELECT UPPER('MotorHead'), 'MotörHead' ILIKE 'MOT%HEAD', REVERSE('MotörHead'), "
                    "REVERSE('🦆ab')") ==
          Rows{{"MOTORHEAD", "true", "daeHrötoM", "ba🦆"}});
}

// ---------------------------------------------------------------------------
// Arguments DuckDB refuses (#34; function/numeric/test_mod.test,
// decimal_mod.test, test_factorial.test, test_gcd_lcm.test,
// test_type_resolution.test, function/string/test_ascii.test,
// test_concat_ws.test, test_contains.test, test_like_escape.test,
// types/decimal/test_decimal.test, types/numeric/float_precision.test).
// Each was accepted and gave a value: NULL, 1, the other argument, a
// byte sequence that is not UTF-8, or a type with its parameter dropped.
// ---------------------------------------------------------------------------

TEST_CASE("Stage 8: functions and operators refuse the arguments DuckDB refuses") {
    Database db;
    Connection conn(db);
    const char *queries[] = {
        "select mod(42, 0)",
        "SELECT 42 % 0",
        "SELECT 10.0 % 0.0",
        "SELECT 2147483647::INTEGER + 1::INTEGER",
        "SELECT 12345678901111111 % 2.0",
        "SELECT factorial(-1)",
        "SELECT factorial(21)",
        "SELECT gcd(42, 'abcd')",
        "SELECT 1::TINYINT + 1::VARCHAR",
        "SELECT CHR(-10)",
        "SELECT CHR(1073741824)",
        "select CONCAT_WS()",
        "select CONCAT_WS(',')",
        "SELECT contains(NULL, NULL)",
        "SELECT '%' LIKE '*' ESCAPE '*'",
        "SELECT '1'::INTEGER(1000)",
        "SELECT CAST(NULL AS FLOAT(0))",
        "SELECT CAST(NULL AS FLOAT(54))",
    };
    for (auto *q : queries) {
        CAPTURE(q);
        CHECK_THROWS(conn.Query(q));
    }
    CHECK(Run(conn, "SELECT mod(42, 5), 42 % 5, factorial(10), gcd(42, 12), lcm(120, 25), "
                    "gcd((-9223372036854775807 - 1)::BIGINT, 1::BIGINT), CHR(937), "
                    "CONCAT_WS(',', 'hello'), '%++' LIKE '*%++' ESCAPE '*'") ==
          Rows{{"2", "2", "3628800", "6", "600", "1", "Ω", "hello", "true"}});
    auto r = conn.Query("SELECT CAST(1 AS FLOAT(24)), CAST(1 AS FLOAT(25))");
    CHECK(r.column_types[0].id() == LogicalTypeId::FLOAT);
    CHECK(r.column_types[1].id() == LogicalTypeId::DOUBLE);
}
