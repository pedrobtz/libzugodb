SELECT *
FROM (VALUES (1, 1)) l(a, x)
RIGHT JOIN (VALUES (2, 1)) r(b, y)
ON #1 = 1
AND #3 = 2
AND EXISTS (
	SELECT 1
	WHERE l.x = r.y
);
