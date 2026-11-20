SELECT 
	i,
	i // 2 AS outside,
	rank(ORDER BY i // 2) OVER w,
	percent_rank(ORDER BY i // 2) OVER w,
FROM range(10) tbl(i)
WINDOW w AS (
	ORDER BY i // 2
	ROWS BETWEEN 3 PRECEDING AND 3 FOLLOWING
)
ORDER BY 1
