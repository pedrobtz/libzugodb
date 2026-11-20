SELECT g,
       count(DISTINCT s) FILTER (WHERE keep),
       count(DISTINCT s) FILTER (WHERE NOT keep)
FROM distinct_rewrite_order
GROUP BY g
ORDER BY g;
