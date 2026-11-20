SELECT o.x, EXISTS (
	SELECT *
	FROM (VALUES (1)) l(a)
	ANTI JOIN (VALUES (2)) r(b)
		ON l.a = r.b AND o.x = 10
) AS ex
FROM (VALUES (10), (20)) o(x)
ORDER BY o.x;
