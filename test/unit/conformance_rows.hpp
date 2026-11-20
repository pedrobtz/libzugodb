#pragma once

// Rows of a query result as text, for the conformance doctests (libzugodb
// Stages 7 and 8): NULL as "NULL", every other value as Value::ToString.

#include "slothdb/main/connection.hpp"

#include <string>
#include <vector>

namespace zugodb::conformance {

using Rows = std::vector<std::vector<std::string>>;

inline Rows Collect(const QueryResult &r) {
    Rows rows;
    for (idx_t i = 0; i < r.RowCount(); i++) {
        std::vector<std::string> row;
        for (idx_t j = 0; j < r.ColumnCount(); j++) {
            const auto &v = r.GetValue(i, j);
            row.push_back(v.IsNull() ? "NULL" : v.ToString());
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

inline Rows Run(Connection &conn, const std::string &sql) { return Collect(conn.Query(sql)); }

} // namespace zugodb::conformance
