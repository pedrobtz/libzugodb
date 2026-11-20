SELECT g, count(DISTINCT s), sum(y) FILTER (WHERE keep)
FROM distinct_rewrite_order
GROUP BY g
ORDER BY g;
