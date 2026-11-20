SELECT o.x, EXISTS (
	SELECT *
	FROM (VALUES (1)) l(a)
	LEFT JOIN (VALUES (1)) r(b)
		ON l.a = r.b + o.x
	WHERE r.b IS NOT NULL
) AS ex
FROM (VALUES (0), (1)) o(x)
ORDER BY o.x;
