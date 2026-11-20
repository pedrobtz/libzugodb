SELECT i, (1, 10) <> ANY (
	SELECT x, y
	FROM (VALUES (1, NULL::INT), (2, 20)) t(x, y)
	WHERE x = i
) FROM (VALUES (1), (2), (3)) outer_t(i)
ORDER BY i;
