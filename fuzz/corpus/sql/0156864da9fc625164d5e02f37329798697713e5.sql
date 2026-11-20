WITH test AS (
	SELECT
		i AS id,
		i AS begin,
		i + 10 AS end,
		i % 2 AS p1,
		i % 3 AS p2
	FROM range(0, 10) tbl(i)
),
sub AS (
	SELECT lhs.id AS lid, rhs.id AS rid
	FROM test lhs, test rhs
	WHERE lhs.begin < rhs.end
	  AND rhs.begin < lhs.end
	  AND lhs.p1 <> rhs.p1
	  AND lhs.p2 <> rhs.p2
	ORDER BY ALL
)
SELECT MIN(lid), MAX(rid)
FROM sub
