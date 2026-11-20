WITH samples AS (
	SELECT col0 AS starts, col1 AS ends
	FROM (VALUES
		(5, 9),
		(10, 13),
		(14, 20),
		(21, 23)
	)
)
SELECT
      s1.starts as s1_starts,
      s2.starts as s2_starts,
FROM samples AS s1 ASOF JOIN samples as s2 ON s2.ends >= (s1.ends - 5)
WHERE s1_starts <> s2_starts
ORDER BY ALL
