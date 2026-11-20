SELECT 
	d1.time,
	d2.time,
	d1.value, 
	d2.value
FROM right_pushdown d1 
ASOF JOIN (
	SELECT * FROM right_pushdown WHERE value is not NULL
	) d2
	ON d1.time >= d2.time
ORDER BY ALL;
