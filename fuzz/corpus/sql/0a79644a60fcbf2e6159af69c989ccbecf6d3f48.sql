SELECT l.a, l.x, r.b
FROM pair_l l
LEFT JOIN pair_r r ON EXISTS (
	SELECT 1 FROM pair_s s WHERE s.a = l.a AND s.b = r.b
)
ORDER BY l.x, r.b NULLS LAST;
