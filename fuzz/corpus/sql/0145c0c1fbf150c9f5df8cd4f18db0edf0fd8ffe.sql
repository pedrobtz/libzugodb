SELECT
	 x
	,y
	,z
	, {binary}(x, 1) OVER (PARTITION BY y) AS plain_window
	, {binary}(x, 1) FILTER (WHERE x = 1) OVER (PARTITION BY y) AS x_filtered_window
	, {binary}(x, 1) FILTER (WHERE z = 0) OVER (PARTITION BY y) AS z_filtered_window
FROM testing
ORDER BY y, x;
