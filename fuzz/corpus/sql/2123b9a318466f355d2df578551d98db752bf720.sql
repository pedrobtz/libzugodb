CREATE TABLE five_complex AS
	SELECT
		1 AS i,
		NULL::VARCHAR AS s,
		NULL::BIGINT[] AS l,
		NULL AS r
        UNION ALL
	SELECT
		i::integer AS i,
		i::VARCHAR AS s,
		[i] AS l,
		{'a': i} AS r
	FROM range(1, 6, 1) t1(i)
