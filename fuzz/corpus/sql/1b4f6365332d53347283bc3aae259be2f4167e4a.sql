SELECT l.a, l.cnt, r.b
FROM (SELECT a, count(*) AS cnt FROM pair_multi_l GROUP BY a) l
FULL OUTER JOIN pair_multi_r r ON EXISTS (
	SELECT 1
	FROM pair_multi_agg_s s
	WHERE s.a = l.a
	  AND s.b = r.b
	  AND s.cnt = l.cnt
)
ORDER BY ALL;
