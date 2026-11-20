SELECT outer_l.x, q.a, q.b
FROM pair_l outer_l
LEFT JOIN LATERAL (
	SELECT l.a, r.b
	FROM pair_l l
	FULL OUTER JOIN pair_r r ON EXISTS (
		SELECT 1
		FROM pair_s s
		WHERE s.a = l.a
		  AND s.b = r.b
		  AND s.a = outer_l.a
	)
	WHERE l.x = outer_l.x
) q ON true
ORDER BY outer_l.x, q.a, q.b NULLS LAST;
