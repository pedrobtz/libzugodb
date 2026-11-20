SELECT outer_l.x, q.lx, q.b
FROM pair_l outer_l
LEFT JOIN LATERAL (
	SELECT l.x AS lx, r.b
	FROM (SELECT * FROM pair_l WHERE pair_l.a = outer_l.a) l
	LEFT JOIN pair_r r ON EXISTS (
		SELECT 1
		FROM pair_s s
		WHERE s.a = l.a
		  AND s.b = r.b
	)
) q ON true
ORDER BY outer_l.x, q.lx, q.b NULLS LAST;
