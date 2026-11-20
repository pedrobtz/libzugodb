SELECT
	 x
	,y
	,z
	, {unary}(x) OVER (PARTITION BY y) AS plain_window
	, {unary}(x) FILTER (WHERE x = 1) OVER (PARTITION BY y) AS x_filtered_window
	, {unary}(x) FILTER (WHERE z = 0) OVER (PARTITION BY y) AS z_filtered_window
FROM testing
ORDER BY y, x;
