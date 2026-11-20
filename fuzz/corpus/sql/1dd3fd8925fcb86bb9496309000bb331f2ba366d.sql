SELECT *
FROM pair_nested_outer_o o
WHERE EXISTS (
	SELECT 1
	FROM pair_nested_l l
	FULL OUTER JOIN pair_nested_r r ON EXISTS (
		SELECT 1
		FROM pair_nested_local_s s
		WHERE s.a = l.a
		  AND s.b = r.b
		  AND s.c = o.x
	)
);
