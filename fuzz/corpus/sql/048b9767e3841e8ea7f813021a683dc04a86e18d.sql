EXPLAIN SELECT l.a, l.x, r.b, r.y
FROM pair_unique_domain_l l
FULL OUTER JOIN pair_unique_domain_r r ON EXISTS (
	SELECT 1
	FROM pair_unique_domain_s s
	WHERE s.a = l.a
	  AND s.b = r.b
);
