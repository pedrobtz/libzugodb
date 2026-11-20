#pragma once

// The engine holds no NaN (libzugodb Stage 8, #33; design L23). DuckDB
// orders NaN above every number and equal to itself; the engine's
// comparisons, sorts, joins, groups and MIN/MAX are IEEE's, in many
// hand-written paths, so a NaN gave answers that were not DuckDB's. NaN
// is refused where one would be made (text, arithmetic, a function, an
// aggregate) and, as a backstop, where one would be stored or returned.
// Infinities are kept: IEEE orders them as DuckDB does. #42 tracks
// DuckDB's NaN semantics.

#include "slothdb/common/exception.hpp"
#include "slothdb/common/types/data_chunk.hpp"
#include "slothdb/common/types/vector.hpp"

#include <cmath>

namespace zugodb {

[[noreturn]] inline void ThrowNaN() {
    throw NotImplementedException(
        "NaN is not supported: the engine does not order or compare it as DuckDB does "
        "(libzugodb#42)");
}

inline void RefuseNaN(const Vector &v, idx_t count) {
    auto id = v.GetType().id();
    if (id != LogicalTypeId::DOUBLE && id != LogicalTypeId::FLOAT) return;
    const auto &valid = v.GetValidity();
    if (id == LogicalTypeId::DOUBLE) {
        const double *d = v.GetData<double>();
        for (idx_t i = 0; i < count; i++) {
            if (std::isnan(d[i]) && valid.RowIsValid(i)) ThrowNaN();
        }
    } else {
        const float *f = v.GetData<float>();
        for (idx_t i = 0; i < count; i++) {
            if (std::isnan(f[i]) && valid.RowIsValid(i)) ThrowNaN();
        }
    }
}

inline void RefuseNaN(const DataChunk &chunk) {
    for (idx_t c = 0; c < chunk.ColumnCount(); c++) {
        RefuseNaN(chunk.GetVector(c), chunk.size());
    }
}

} // namespace zugodb
