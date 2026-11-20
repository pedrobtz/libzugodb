SELECT l.a, r.b
FROM (VALUES (1), (2)) l(a)
FULL OUTER JOIN range(10, 40, 10) r(b) ON EXISTS (
	SELECT 1
	FROM pair_multi_s s
	WHERE s.a = l.a
	  AND s.b = r.b
)
ORDER BY ALL;
