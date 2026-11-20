#pragma once

// A Parquet file as a table, set up through the catalog's C++ API, for the
// doctests of the Parquet reader and its scan operator. SQL cannot name a
// file since Stage 2 (#4); this does what read_parquet() did before: a
// catalog entry with the file's schema, its path and its parsed footer, so
// that a query of `name` runs PhysicalParquetScan and its parallel paths.
// It goes with the reader in Stage 11.

#include "slothdb/catalog/catalog.hpp"
#include "slothdb/catalog/table_catalog_entry.hpp"
#include "slothdb/main/database.hpp"
#include "slothdb/storage/data_table.hpp"
#include "slothdb/storage/parquet.hpp"

#include <memory>
#include <string>
#include <vector>

namespace zugodb {
namespace testing {

inline void AddParquetTable(Database &db, const std::string &name, const std::string &path) {
    auto reader = std::make_shared<ParquetReader>(path);
    auto names = reader->GetColumnNames();
    auto types = reader->GetColumnTypes();
    std::vector<ColumnDefinition> cols;
    for (size_t i = 0; i < names.size(); i++) cols.emplace_back(names[i], types[i]);
    auto &entry = db.GetCatalog().CreateTable(name, std::move(cols));
    entry.SetStorage(std::make_shared<DataTable>(types));
    entry.SetParquetPath(path);
    entry.SetCachedParquetReader(std::move(reader));
}

} // namespace testing
} // namespace zugodb
