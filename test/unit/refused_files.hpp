#pragma once

// Reading or writing files from SQL is refused (libzugodb Stage 2, #4;
// design §2): files reach the engine as Arrow C streams, through zugodb.
// Every doctest that gives SQL a file path goes through CheckRefused, so a
// grep for file routes in test/ finds only its arguments.

#include "doctest.h"
#include "slothdb/common/exception.hpp"
#include "slothdb/main/connection.hpp"

#include <string>

namespace zugodb {
namespace testing {

inline const std::string FILE_REFUSAL = "Reading or writing files from SQL is not supported";

// `sql` is refused with the file refusal, not another error (a missing
// file, a syntax error), and the same connection then answers a query.
inline void CheckRefused(Connection &conn, const std::string &sql) {
    INFO(sql);
    std::string what;
    bool refused = false;
    try {
        conn.Query(sql);
        what = "(no error)";
    } catch (const NotImplementedException &e) {
        what = e.what();
        refused = what.find(FILE_REFUSAL) != std::string::npos;
    } catch (const std::exception &e) {
        what = e.what();
    }
    INFO(what);
    CHECK(refused);
    auto r = conn.Query("SELECT 41 + 1");
    REQUIRE(r.RowCount() == 1);
    CHECK(r.GetValue(0, 0).ToString() == "42");
}

} // namespace testing
} // namespace zugodb
