// libFuzzer harness for Checkpoint::Load (roadmap Stage 4).
//
// Each input is a database file image. Before it is opened, the harness
// makes it pass the two checks a mutated file would almost never pass by
// chance, so the fuzzer reaches the parser behind them:
//   - catalog_size (offset 24) is set to the file's size minus
//     catalog_offset (offset 16), when that offset is inside the file;
//   - the CRC-32 at offset 12 is recomputed with Checkpoint::ComputeChecksum.
// The magic and the version are left as the input has them.
//
// The oracle, for a file that opens:
//   - each table's row count, as the catalog has it, equals SELECT count(*)
//     and the rows of SELECT *, and its rows scan from storage;
//   - the database is closed, which saves it, and opened again: the second
//     open succeeds and gives the same tables and the same rows.
// A table is named in SQL as a quoted identifier, its quotes doubled
// (Stage 9 gave the tokenizer the "" escape).
// A refusal (an exception from the Database constructor) is a pass. Any
// crash, hang, sanitizer report or broken oracle fails (__builtin_trap).
//
// Build: cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
//        -DZUGODB_FUZZ=ON -DZUGODB_BUILD_TESTS=OFF && cmake --build build-fuzz
// Run:   build-fuzz/fuzz/checkpoint_load_fuzzer fuzz/corpus/checkpoint_load

#include "zugodb/main/connection.hpp"
#include "zugodb/main/database.hpp"
#include "zugodb/storage/checkpoint.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

using namespace zugodb;

namespace {

void Check(bool ok) {
    if (!ok) __builtin_trap();
}

const std::string &DbPath() {
    static const std::string path = [] {
        const char *dir = std::getenv("TMPDIR");
        std::string base = (dir && *dir) ? dir : "/tmp";
        if (base.back() != '/') base += '/';
        return base + "checkpoint_load_fuzz_" + std::to_string(getpid()) + ".db";
    }();
    return path;
}

void RemoveFiles() {
    std::remove(DbPath().c_str());
    std::remove((DbPath() + ".tmp").c_str());
}

bool WriteFile(const std::vector<uint8_t> &bytes) {
    FILE *f = std::fopen(DbPath().c_str(), "wb");
    if (!f) return false;
    bool ok = bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    return std::fclose(f) == 0 && ok;
}

std::string QuoteIdentifier(const std::string &name) {
    std::string out = "\"";
    for (char c : name) {
        out += c;
        if (c == '"') out += '"';
    }
    return out + "\"";
}

std::string Text(const Value &v) {
    if (v.IsNull()) return "N|";
    auto s = v.ToString();
    return std::to_string(s.size()) + ":" + s + "|";
}

// Every table of the database, its columns and its rows in storage order,
// as text; each table's row counts are checked on the way.
std::string Snapshot(Database &db) {
    auto &catalog = db.GetCatalog();
    auto names = catalog.GetSchema().GetTableNames();
    std::sort(names.begin(), names.end());
    Connection conn(db);
    std::string out;
    for (auto &name : names) {
        auto *entry = catalog.GetTable(name);
        Check(entry != nullptr);
        auto rows = entry->GetStorage().Count();
        out += std::to_string(name.size()) + ":" + name + " " + std::to_string(rows) + "\n";
        for (auto &col : entry->GetColumns()) {
            out += std::to_string(col.name.size()) + ":" + col.name + " " + col.type.ToString() + "\n";
        }

        auto &storage = entry->GetStorage();
        auto types = entry->GetTypes();
        auto state = storage.InitScan();
        idx_t scanned = 0;
        while (true) {
            DataChunk chunk;
            chunk.Initialize(types);
            if (!storage.Scan(state, chunk)) break;
            for (idx_t r = 0; r < chunk.size(); r++) {
                for (idx_t c = 0; c < types.size(); c++) out += Text(chunk.GetValue(c, r));
                out += '\n';
            }
            scanned += chunk.size();
        }
        Check(scanned == rows);

        auto table = QuoteIdentifier(name);
        auto count = conn.Query("SELECT count(*) FROM " + table);
        Check(count.RowCount() == 1);
        Check(static_cast<idx_t>(count.GetValue(0, 0).GetValue<int64_t>()) == rows);
        auto all = conn.Query("SELECT * FROM " + table);
        Check(all.RowCount() == rows);
        Check(all.ColumnCount() == types.size());
        for (idx_t r = 0; r < all.RowCount(); r++) {
            for (idx_t c = 0; c < all.ColumnCount(); c++) Text(all.GetValue(r, c));
        }
    }
    return out;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    std::vector<uint8_t> image(data, data + size);
    if (size >= sizeof(FileHeader)) {
        uint64_t catalog_offset;
        std::memcpy(&catalog_offset, image.data() + offsetof(FileHeader, catalog_offset), 8);
        if (catalog_offset <= size) {
            uint64_t catalog_size = size - catalog_offset;
            std::memcpy(image.data() + offsetof(FileHeader, catalog_size), &catalog_size, 8);
        }
        uint32_t crc = Checkpoint::ComputeChecksum(image.data(), image.size());
        std::memcpy(image.data() + FILE_CHECKSUM_OFFSET, &crc, 4);
    }

    RemoveFiles();
    Check(WriteFile(image));

    std::string first;
    {
        std::unique_ptr<Database> db;
        try {
            db = std::make_unique<Database>(DbPath());
        } catch (const std::exception &) {
            RemoveFiles();
            return 0; // refused: fine
        }
        first = Snapshot(*db);
    } // closing saves the database over the input

    DatabaseConfig config;
    config.path = DbPath();
    config.read_only = true;
    std::string second;
    {
        // A file Save wrote from what Load read must open again.
        Database db(config);
        second = Snapshot(db);
    }
    Check(first == second);
    RemoveFiles();
    return 0;
}
