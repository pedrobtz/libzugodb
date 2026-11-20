SELECT i, ARRAY(
	SELECT * FROM integers WHERE i>i1.i ORDER BY i
) top
FROM integers i1
ORDER BY i
