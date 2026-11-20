SELECT mode(DISTINCT v ORDER BY v), mode(DISTINCT v ORDER BY v DESC)
FROM mode_rewrite
WHERE g = 1;
