SELECT g, count(DISTINCT x) FILTER (WHERE y > 3)
FROM distinct_rewrite_nulls
GROUP BY g
ORDER BY g NULLS FIRST;
