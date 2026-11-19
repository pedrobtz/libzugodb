/*
 * SlothDB CLI Shell
 *
 * Interactive SQL REPL for SlothDB.
 * Usage: slothdb [database_path]
 *        slothdb -c "SELECT ..."
 */

#include "slothdb/main/database.hpp"
#include "slothdb/main/connection.hpp"
#include "slothdb/catalog/catalog.hpp"
#include "slothdb/catalog/schema_catalog_entry.hpp"
#include "slothdb/catalog/table_catalog_entry.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <exception>
#include <iostream>
#include <memory>
#include <vector>

#ifdef SLOTHDB_HAS_LINENOISE
extern "C" {
#include "linenoise.h"
}
#endif

using zugodb::Connection;
using zugodb::Database;
using zugodb::QueryResult;

#ifndef SLOTHDB_SHELL_VERSION
#define SLOTHDB_SHELL_VERSION "unknown"
#endif

static const char *kHistoryFile = ".slothdb_history";

// An open database and its one connection. An empty path opens an
// in-memory database.
struct Session {
    std::unique_ptr<Database> db;
    std::unique_ptr<Connection> conn;
};

static bool open_session(const std::string &path, Session &out) {
    try {
        Session s;
        s.db = path.empty() ? std::make_unique<Database>()
                            : std::make_unique<Database>(path);
        s.conn = std::make_unique<Connection>(*s.db);
        out = std::move(s);
        return true;
    } catch (...) {
        return false;
    }
}

static void print_result(const QueryResult &result) {
    uint64_t cols = result.ColumnCount();
    uint64_t rows = result.RowCount();

    if (cols == 0) {
        printf("OK\n");
        return;
    }

    for (uint64_t c = 0; c < cols; c++) {
        if (c > 0) printf(" | ");
        printf("%-15s", result.column_names[c].c_str());
    }
    printf("\n");

    for (uint64_t c = 0; c < cols; c++) {
        if (c > 0) printf("-+-");
        printf("---------------");
    }
    printf("\n");

    for (uint64_t r = 0; r < rows; r++) {
        for (uint64_t c = 0; c < cols; c++) {
            if (c > 0) printf(" | ");
            auto &val = result.GetValue(r, c);
            printf("%-15s", val.IsNull() ? "NULL" : val.ToString().c_str());
        }
        printf("\n");
    }

    printf("(%llu row%s)\n", (unsigned long long)rows, rows == 1 ? "" : "s");
}

static void print_help() {
    printf("\n");
    printf("  Dot commands\n");
    printf("  ------------\n");
    printf("  .help                       Show this help\n");
    printf("  .quit / .exit               Exit the shell\n");
    printf("  .version                    Print SlothDB version\n");
    printf("  .clear                      Clear the screen\n");
    printf("  .open <path>                Close current DB and open a new one\n");
    printf("                                :memory:        in-process, non-persistent\n");
    printf("                                file.slothdb    single-file persistent DB\n");
    printf("  .tables [PATTERN]           List tables. PATTERN is SQL LIKE ('sales%%').\n");
    printf("  .schema [TABLE]             Show CREATE TABLE statements. No arg = all tables.\n");
    printf("\n");
    printf("  Query any file directly (no import step needed)\n");
    printf("  ------------------------------------------------\n");
    printf("  SELECT * FROM 'data.csv';\n");
    printf("  SELECT * FROM 'logs.json';\n");
    printf("  SELECT * FROM read_parquet('events.parquet');\n");
    printf("  SELECT * FROM read_xlsx('report.xlsx');\n");
    printf("  SELECT * FROM read_avro('events.avro');\n");
    printf("  SELECT * FROM read_arrow('stream.arrow');\n");
    printf("  SELECT * FROM sqlite_scan('app.db', 'users');\n");
    printf("  SELECT * FROM 'https://host/data.csv';    -- HTTPS and s3:// work too\n");
    printf("\n");
    printf("  Materialize / transform with CTAS\n");
    printf("  ---------------------------------\n");
    printf("  CREATE TABLE sales AS SELECT * FROM 'sales.csv';\n");
    printf("  CREATE OR REPLACE TABLE top AS\n");
    printf("    SELECT region, SUM(revenue) AS rev FROM sales GROUP BY region;\n");
    printf("  CREATE TABLE IF NOT EXISTS archive AS SELECT * FROM sales WHERE year < 2024;\n");
    printf("\n");
    printf("  Virtual and live views\n");
    printf("  ----------------------\n");
    printf("  CREATE VIEW v AS SELECT ... ;            -- re-executes on every SELECT\n");
    printf("  CREATE LIVE VIEW lv AS SELECT * FROM 'app.log';\n");
    printf("      -- cached; invalidates when the source file's mtime changes.\n");
    printf("\n");
    printf("  Export with COPY\n");
    printf("  ----------------\n");
    printf("  COPY sales TO 'out.csv';\n");
    printf("  COPY (SELECT * FROM sales WHERE region='EU') TO 'eu.parquet' (FORMAT PARQUET);\n");
    printf("\n");
    printf("  Catalog introspection\n");
    printf("  ---------------------\n");
    printf("  PRAGMA table_info('sales');           -- column name/type/notnull/pk\n");
    printf("  PRAGMA database_list;                 -- attached DBs (currently just one)\n");
    printf("  DESCRIBE SELECT ... ;                 -- result schema of a query\n");
    printf("  EXPLAIN SELECT ... ;                  -- logical plan\n");
    printf("\n");
    printf("  Input handling\n");
    printf("  --------------\n");
    printf("  Statements run when they end with a semicolon.\n");
    printf("  Blank line cancels a partial statement.\n");
#ifdef SLOTHDB_HAS_LINENOISE
    printf("  Arrow keys move the cursor; Up/Down browses history.\n");
    printf("  History persists in .slothdb_history in the current directory.\n");
    printf("  Ctrl+C cancels input. Ctrl+D at empty prompt exits.\n");
#else
    printf("  On Windows, use cmd.exe's native line editing. Ctrl+C exits.\n");
#endif
    printf("\n");
    printf("  Docs: https://github.com/SouravRoy-ETL/slothdb\n");
    printf("\n");
}

// Run a query string, print its result (or error). Returns true on success.
static bool run_query(Connection &conn, const std::string &sql) {
    try {
        auto result = conn.Query(sql);
        print_result(result);
        return true;
    } catch (const std::exception &e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return false;
    }
}


// Simple wildcard match: '%' matches any run of chars. Used for the
// optional LIKE pattern on .tables.
static bool like_match(const std::string &name, const std::string &pattern) {
    // Iterative DP-free matcher for '%' only (no '_' / escapes needed here).
    size_t ni = 0, pi = 0, star_n = std::string::npos, star_p = 0;
    while (ni < name.size()) {
        if (pi < pattern.size() && pattern[pi] == '%') {
            star_p = ++pi;
            star_n = ni;
        } else if (pi < pattern.size() && pattern[pi] == name[ni]) {
            pi++; ni++;
        } else if (star_n != std::string::npos) {
            pi = star_p;
            ni = ++star_n;
        } else {
            return false;
        }
    }
    while (pi < pattern.size() && pattern[pi] == '%') pi++;
    return pi == pattern.size();
}

// Enumerate tables via the catalog - information_schema isn't a real
// schema in SlothDB's parser, so SQL-based introspection doesn't work.
static void handle_tables(Connection &conn, const std::string &arg) {
    std::string pattern = arg;
    if (pattern.size() >= 2 && (pattern.front() == '\'' || pattern.front() == '"')) {
        pattern = pattern.substr(1, pattern.size() - 2);
    }
    auto names = conn.GetCatalog().GetSchema().GetTableNames();
    uint64_t n = names.size();
    if (n == 0) {
        printf("(no tables)\n");
        return;
    }
    printf("%-32s\n", "table_name");
    printf("--------------------------------\n");
    uint64_t shown = 0;
    for (uint64_t i = 0; i < n; i++) {
        const std::string &sname = names[i];
        if (!pattern.empty() && !like_match(sname, pattern)) continue;
        printf("%-32s\n", sname.c_str());
        shown++;
    }
    printf("(%llu table%s)\n", (unsigned long long)shown, shown == 1 ? "" : "s");
}

// Show columns for a specific table (or every table when arg is empty)
// via the catalog.
static void handle_schema(Connection &conn, const std::string &arg) {
    std::string name = arg;
    if (name.size() >= 2 && (name.front() == '\'' || name.front() == '"')) {
        name = name.substr(1, name.size() - 2);
    }
    auto &schema = conn.GetCatalog().GetSchema();
    auto names = schema.GetTableNames();
    uint64_t n = names.size();
    if (n == 0) { printf("(no tables)\n"); return; }

    auto print_table = [&](uint64_t ti) {
        auto *tbl = schema.GetTable(names[ti]);
        if (!tbl) return;
        printf("%s (\n", names[ti].c_str());
        auto &columns = tbl->GetColumns();
        uint64_t cn = columns.size();
        for (uint64_t j = 0; j < cn; j++) {
            printf("  %-24s %s%s\n",
                   columns[j].name.c_str(),
                   columns[j].type.ToString().c_str(),
                   (j + 1 == cn) ? "" : ",");
        }
        printf(");\n");
    };

    if (name.empty()) {
        for (uint64_t i = 0; i < n; i++) { print_table(i); if (i + 1 < n) printf("\n"); }
        return;
    }
    for (uint64_t i = 0; i < n; i++) {
        if (name == names[i]) { print_table(i); return; }
    }
    printf("(no table named '%s')\n", name.c_str());
}

// SQL keyword completion (for linenoise TAB).
#ifdef SLOTHDB_HAS_LINENOISE
static const char *kSqlKeywords[] = {
    "SELECT", "FROM", "WHERE", "GROUP BY", "ORDER BY", "HAVING", "LIMIT",
    "INSERT INTO", "UPDATE", "DELETE FROM", "CREATE TABLE", "CREATE VIEW",
    "DROP TABLE", "ALTER TABLE", "JOIN", "LEFT JOIN", "RIGHT JOIN",
    "INNER JOIN", "CROSS JOIN", "UNION", "INTERSECT", "EXCEPT",
    "WITH", "QUALIFY", "OVER", "PARTITION BY",
    "read_csv(", "read_parquet(", "read_json(", "read_xlsx(", "read_avro(",
    "sqlite_scan(",
    ".help", ".quit", ".tables", ".schema", ".version", ".open", ".clear",
    nullptr,
};

static void slothdb_complete(const char *buf, linenoiseCompletions *lc) {
    size_t buf_len = strlen(buf);
    if (buf_len == 0) return;
    // Find the last whitespace - we complete the word after it.
    const char *word = buf;
    for (size_t i = buf_len; i-- > 0; ) {
        if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n') {
            word = buf + i + 1;
            break;
        }
    }
    size_t word_len = strlen(word);
    if (word_len == 0) return;
    for (int i = 0; kSqlKeywords[i]; i++) {
        const char *kw = kSqlKeywords[i];
        if (strncasecmp(word, kw, word_len) == 0) {
            // Rebuild: prefix (everything before the word) + keyword.
            std::string candidate(buf, word - buf);
            candidate += kw;
            linenoiseAddCompletion(lc, candidate.c_str());
        }
    }
}
#endif

// Strip invisible UTF-8 characters that sneak into pasted input and break
// parsing silently:
//   U+200B..U+200F  ZWSP / ZWNJ / ZWJ / LRM / RLM
//   U+202A..U+202E  LRE / RLE / PDF / LRO / RLO
//   U+2066..U+2069  LRI / RLI / FSI / PDI
//   U+FEFF          BOM / zero-width no-break space
//   U+00AD          soft hyphen
// Windows File Explorer's "Copy as path" injects U+202A before the drive
// letter; the user sees `'C:\...'` but the shell sees `'<LRE>C:\...'` and
// the CSV reader reports a confusing "file not found." Cheap to skip before
// parsing, zero cost when absent.
static std::string strip_invisible(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    const auto n = s.size();
    for (size_t i = 0; i < n; ) {
        unsigned char b0 = static_cast<unsigned char>(s[i]);
        if (b0 == 0xE2 && i + 2 < n) {
            unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
            unsigned char b2 = static_cast<unsigned char>(s[i + 2]);
            if (b1 == 0x80 && ((b2 >= 0x8B && b2 <= 0x8F) ||
                               (b2 >= 0xAA && b2 <= 0xAE))) {
                i += 3; continue;
            }
            if (b1 == 0x81 && b2 >= 0xA6 && b2 <= 0xA9) {
                i += 3; continue;
            }
        }
        if (b0 == 0xEF && i + 2 < n &&
            static_cast<unsigned char>(s[i + 1]) == 0xBB &&
            static_cast<unsigned char>(s[i + 2]) == 0xBF) {
            i += 3; continue;
        }
        if (b0 == 0xC2 && i + 1 < n &&
            static_cast<unsigned char>(s[i + 1]) == 0xAD) {
            i += 2; continue;
        }
        out.push_back(s[i]);
        i++;
    }
    return out;
}

// Read one logical line, falling back to getline on Windows. Returns false on
// EOF / Ctrl+D. Strips invisible BIDI / ZWSP / BOM marks from the result so
// pasted file paths with hidden control characters parse the same as the
// visible text the user sees.
static bool read_line(const char *prompt, std::string &out) {
#ifdef SLOTHDB_HAS_LINENOISE
    char *raw = linenoise(prompt);
    if (!raw) return false;
    out = strip_invisible(raw);
    if (!out.empty()) linenoiseHistoryAdd(out.c_str());
    linenoiseFree(raw);
    return true;
#else
    printf("%s", prompt);
    fflush(stdout);
    if (!std::getline(std::cin, out)) return false;
    out = strip_invisible(out);
    return true;
#endif
}

int main(int argc, char *argv[]) {
    const char *db_path = "";
    const char *cmd_query = nullptr;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            cmd_query = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: slothdb [database_path]\n");
            printf("       slothdb -c \"SELECT ...\"\n");
            printf("Opens an in-memory database if no path is given.\n");
            return 0;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("%s\n", SLOTHDB_SHELL_VERSION);
            return 0;
        } else {
            db_path = argv[i];
        }
    }

    if (!cmd_query) {
        printf("SlothDB Shell v%s\n", SLOTHDB_SHELL_VERSION);
        printf("Type .help for help, .quit to exit.\n\n");
    }

    Session session;
    if (!open_session(db_path, session)) {
        fprintf(stderr, "Failed to open database: %s\n", db_path);
        return 1;
    }

    // One-shot command: -c "..."
    if (cmd_query) {
        return run_query(*session.conn, cmd_query) ? 0 : 1;
    }

    if (strlen(db_path) > 0) printf("Connected to: %s\n", db_path);
    else                      printf("Connected to in-memory database\n");

#ifdef SLOTHDB_HAS_LINENOISE
    linenoiseHistorySetMaxLen(1000);
    linenoiseHistoryLoad(kHistoryFile);
    linenoiseSetCompletionCallback(slothdb_complete);
#endif

    std::string buffer;
    while (true) {
        const char *prompt = buffer.empty() ? "slothdb> " : "   ...> ";

        std::string line;
        if (!read_line(prompt, line)) break; // EOF / Ctrl+D.

        // Dot commands (only at start of a fresh buffer).
        if (buffer.empty() && !line.empty() && line[0] == '.') {
            // Split on first whitespace: command + optional argument.
            size_t sp = line.find_first_of(" \t");
            std::string cmd = (sp == std::string::npos) ? line : line.substr(0, sp);
            std::string arg = (sp == std::string::npos) ? "" : line.substr(sp + 1);
            // Trim leading whitespace from arg.
            while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t'))
                arg.erase(arg.begin());

            if (cmd == ".quit" || cmd == ".exit") break;
            if (cmd == ".help") { print_help(); continue; }
            if (cmd == ".version") { printf("%s\n", SLOTHDB_SHELL_VERSION); continue; }
            if (cmd == ".tables") { handle_tables(*session.conn, arg); continue; }
            if (cmd == ".schema") { handle_schema(*session.conn, arg); continue; }
            if (cmd == ".clear") { printf("\033[2J\033[H"); fflush(stdout); continue; }
            if (cmd == ".open") {
                if (arg.empty()) { printf("Usage: .open <path>\n"); continue; }
                // Open the new database FIRST; swap in only on success.
                // Previously this closed the old db before attempting the
                // new open, so a failure left the shell with invalid
                // handles and `return 1`'d the whole process - exiting
                // the REPL instead of returning to the prompt.
                Session next;
                if (!open_session(arg, next)) {
                    fprintf(stderr, "Failed to open: %s\n", arg.c_str());
                    // Hint if the user handed us what looks like a CSV/Parquet.
                    auto ends_with = [&](const char *suf) {
                        size_t n = strlen(suf);
                        return arg.size() >= n &&
                               arg.compare(arg.size() - n, n, suf) == 0;
                    };
                    if (ends_with(".csv") || ends_with(".tsv") ||
                        ends_with(".parquet") || ends_with(".json") ||
                        ends_with(".arrow") || ends_with(".xlsx")) {
                        fprintf(stderr,
                            "Hint: .open is for persistent .slothdb files. "
                            "To query a data file, use:\n"
                            "  SELECT * FROM '%s';\n", arg.c_str());
                    }
                    continue;  // back to the prompt, old db still active
                }
                // Success - dispose of the old handles and swap.
                // The connection refers to its database: drop it first.
                session.conn.reset();
                session.db.reset();
                session = std::move(next);
                printf("Connected to: %s\n", arg.c_str());
                continue;
            }
            printf("Unknown command: %s  (type .help for help)\n", line.c_str());
            continue;
        }

        buffer += line + " ";

        auto trimmed = buffer;
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t'))
            trimmed.pop_back();
        if (trimmed.empty()) { buffer.clear(); continue; }
        if (trimmed.back() != ';') continue; // Keep reading multi-line input.

        run_query(*session.conn, buffer);
        buffer.clear();
    }

#ifdef SLOTHDB_HAS_LINENOISE
    linenoiseHistorySave(kHistoryFile);
#endif

    printf("\nBye!\n");
    return 0;
}
