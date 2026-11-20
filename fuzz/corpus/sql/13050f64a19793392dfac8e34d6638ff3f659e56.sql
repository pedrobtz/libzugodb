SELECT l.a, r.b, count(*)::INTEGER
FROM pair_left_null_l l
LEFT JOIN pair_left_null_r r ON EXISTS (
	SELECT 1
	FROM pair_left_null_s s
	WHERE s.a IS NOT DISTINCT FROM l.a
	  AND s.b IS NOT DISTINCT FROM r.b
)
GROUP BY ALL
ORDER BY ALL;
