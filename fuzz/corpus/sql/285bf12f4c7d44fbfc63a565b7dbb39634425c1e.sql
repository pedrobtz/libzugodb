WITH alternate AS (
	SELECT 
		range r,
		IF(range < 2100, NULL, range) s
	FROM range(3000)
)
SELECT 
	r,
	s,
	first(s IGNORE NULLS) over(ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) f
FROM alternate
QUALIFY (f IS NOT NULL AND r < 2100)
     OR (f IS NULL AND r >= 2100)
