SELECT l.a, l.x, r.b
FROM pair_l l
LEFT JOIN pair_r r ON l.x > 10 AND r.b IN (
	SELECT s.b FROM pair_s s WHERE s.a = l.a
)
ORDER BY l.x, r.b NULLS LAST;
