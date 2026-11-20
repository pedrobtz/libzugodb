SELECT GROUPING(g), g, count(DISTINCT x)
FROM distinct_rewrite_nulls
GROUP BY g
ORDER BY g NULLS FIRST;
