SELECT *
FROM (VALUES ({'v': 1})) l(s)
LEFT JOIN (VALUES ({'v': 2})) r(s)
ON EXISTS (
	SELECT 1
	WHERE s.v = l.s.v
	  AND r.s.v = 2
);
