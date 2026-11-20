SELECT l.a, l2.c, r.b, r2.c
FROM pair_multi_l l
JOIN pair_multi_l2 l2 ON true
FULL OUTER JOIN (
	pair_multi_r r
	JOIN pair_multi_l2 r2 ON true
) ON EXISTS (
	SELECT 1
	FROM pair_multi_s s
	WHERE s.a = l.a
	  AND s.b = r.b
	  AND s.c = l2.c
	  AND s.c = r2.c
)
ORDER BY ALL;
