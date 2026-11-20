SELECT count(*), count(*) FILTER (
    WHERE last_v IS DISTINCT FROM i - i % 4
       OR last_s IS DISTINCT FROM (i - i % 4)::VARCHAR
       OR first_i IS DISTINCT FROM 0
       OR last_null IS NOT NULL
       OR n IS DISTINCT FROM i + 1
)
FROM (
    SELECT i,
           last_value(v IGNORE NULLS) OVER w AS last_v,
           last_value(s IGNORE NULLS) OVER w AS last_s,
           first_value(i) OVER w AS first_i,
           last_value(null_v IGNORE NULLS) OVER w AS last_null,
           count(*) OVER w AS n
    FROM collection_input
    WINDOW w AS (ORDER BY i ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW)
) t;
