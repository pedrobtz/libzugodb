SELECT g, mode(v) FILTER (WHERE keep)
FROM mode_rewrite
WHERE g <> 2
GROUP BY g
ORDER BY g;
