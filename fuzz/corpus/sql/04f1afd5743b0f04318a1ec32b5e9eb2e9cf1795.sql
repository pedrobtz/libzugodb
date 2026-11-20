WITH t AS (
	SELECT i, RANK() OVER (ORDER BY i % 50) AS d
	FROM range(3000) tbl(i)
), w AS (
	SELECT d, COUNT(*) as c
	FROM t
	GROUP BY ALL
)
SELECT COUNT(*), MIN(d), MAX(d), MIN(c), MAX(c)
FROM w
