SELECT outer_r.b, q.a, q.x, q.b
FROM pair_r outer_r
LEFT JOIN LATERAL (
	WITH cr AS (SELECT b, y FROM pair_r WHERE pair_r.b = outer_r.b)
	SELECT l.a, l.x, cr.b
	FROM pair_l l
	RIGHT JOIN cr ON EXISTS (
		SELECT 1
		FROM pair_s s
		WHERE s.a = l.a
		  AND s.b = cr.b
	)
) q ON true
ORDER BY outer_r.b, q.a, q.x, q.b NULLS LAST;
