WITH t(r, i, p, f) AS (VALUES
	(0, 0, 1, 1),
	(1, 1, 1, 1),
	(2, 2, 1, 1),
	(3, 0, 1, 1),
	(4, 1, 1, 1),
	(5, 2, 1, 1)
)
SELECT r, MEDIAN(i) OVER (ORDER BY r ROWS BETWEEN p PRECEDING and f FOLLOWING)
FROM t
ORDER BY 1
