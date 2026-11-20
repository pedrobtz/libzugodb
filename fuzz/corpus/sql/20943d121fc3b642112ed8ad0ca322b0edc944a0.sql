SELECT g,
       count(DISTINCT s) FILTER (WHERE keep),
       string_agg(DISTINCT s, ',' ORDER BY s DESC) FILTER (WHERE keep)
FROM distinct_rewrite_order
GROUP BY g
ORDER BY g;
