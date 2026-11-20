SELECT l.a, l.tags::VARCHAR, l.st::VARCHAR, r.b, r.m::VARCHAR
FROM pair_complex_l l
FULL OUTER JOIN pair_complex_r r ON EXISTS (
	SELECT 1
	FROM pair_complex_s s
	WHERE s.a = l.a
	  AND s.b = r.b
)
ORDER BY l.a NULLS LAST, r.b NULLS LAST;
