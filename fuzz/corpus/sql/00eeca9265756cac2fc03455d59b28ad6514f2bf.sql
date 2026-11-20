WITH t AS (
	SELECT i, DENSE_RANK() OVER (PARTITION BY i // 3000 ORDER BY i % 50) AS d
	FROM range(9000) tbl(i)
), w AS (
	SELECT d, COUNT(*) as c
	FROM t
	GROUP BY ALL
)
SELECT COUNT(*), MIN(d), MAX(d), MIN(c), MAX(c)
FROM w
