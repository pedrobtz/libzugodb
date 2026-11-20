SELECT l.a, l.x, r.b
FROM pair_l l
LEFT JOIN pair_r r ON EXISTS (
	SELECT 1
	FROM pair_left_nested_t t
	WHERE EXISTS (
		SELECT 1
		WHERE EXISTS (
			SELECT 1
			FROM pair_left_multi_s s
			WHERE s.a = l.a
			  AND s.b = r.b
			  AND s.c = t.c
		)
	)
)
ORDER BY l.a, l.x, r.b NULLS LAST;
