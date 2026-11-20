SELECT l.a, l.x, r.b, r.y
FROM (SELECT a, max(x)::INTEGER AS x FROM pair_unique_domain_l GROUP BY a) l
FULL OUTER JOIN (SELECT b, max(y)::INTEGER AS y FROM pair_unique_domain_r GROUP BY b) r ON EXISTS (
	SELECT 1
	FROM pair_unique_domain_s s
	WHERE s.a = l.a
	  AND s.b = r.b
);
