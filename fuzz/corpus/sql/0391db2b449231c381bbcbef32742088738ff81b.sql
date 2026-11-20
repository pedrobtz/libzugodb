SELECT l.a, l.c, r.b
FROM (pair_using_l JOIN pair_using_l2 USING (k)) l
FULL OUTER JOIN pair_multi_r r ON EXISTS (
	SELECT 1
	FROM pair_using_s s
	WHERE s.a = l.a
	  AND s.b = r.b
	  AND s.c = l.c
)
ORDER BY ALL;
