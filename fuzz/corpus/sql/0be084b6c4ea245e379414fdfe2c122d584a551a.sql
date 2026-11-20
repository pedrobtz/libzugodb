SELECT *
FROM (VALUES (1)) l(a)
LEFT JOIN (VALUES (2)) r(a)
ON EXISTS (
	SELECT 1
	WHERE a = l.a
	  AND r.a = 2
);
