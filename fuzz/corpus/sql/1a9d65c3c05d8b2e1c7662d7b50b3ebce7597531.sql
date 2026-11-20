SELECT o.b, q.a, q.b
FROM (VALUES (1), (2)) o(b)
LEFT JOIN LATERAL (
	SELECT l.a, r.b
	FROM LATERAL (SELECT b) l(a)
	RIGHT JOIN (VALUES (1)) r(b) ON EXISTS (
		SELECT 1 WHERE l.a = r.b
	)
) q ON true
ORDER BY o.b;
