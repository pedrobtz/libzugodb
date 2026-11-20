SELECT typeof(lttb(x::FLOAT, y::DOUBLE, 4 ORDER BY x)), typeof(lttb(x::DOUBLE, y::FLOAT, 4 ORDER BY x))
FROM (SELECT i AS x, (i * i) AS y FROM range(10) t(i))
