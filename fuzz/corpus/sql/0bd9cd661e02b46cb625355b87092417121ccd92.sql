SELECT g, string_agg(DISTINCT s, ',' ORDER BY s DESC)
FROM distinct_rewrite_order
GROUP BY g
ORDER BY g;
