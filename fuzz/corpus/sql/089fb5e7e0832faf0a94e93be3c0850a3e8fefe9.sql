SELECT l.a, r.b
FROM pair_l l
FULL OUTER JOIN pair_r r ON NOT EXISTS (
	SELECT 1 FROM pair_s s WHERE s.a = l.a AND s.b = r.b
)
ORDER BY l.a NULLS LAST, r.b NULLS LAST;
