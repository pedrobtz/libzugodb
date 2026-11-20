SELECT l.a, r.b
FROM pair_decor_l l
FULL OUTER JOIN pair_decor_r r ON EXISTS (
	SELECT s.a
	FROM pair_decor_s s
	WHERE s.a = l.a
	  AND s.b = r.b
	GROUP BY s.a
	HAVING count(*) > 0
)
ORDER BY ALL;
