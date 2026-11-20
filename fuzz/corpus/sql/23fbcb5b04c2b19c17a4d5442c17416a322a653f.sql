SELECT count(*), count(*) FILTER (l2.g <> r.g) FROM l2 ASOF JOIN r ON union_value(k := l2.g) = union_value(k := r.g) AND l2.ts >= r.ts;
