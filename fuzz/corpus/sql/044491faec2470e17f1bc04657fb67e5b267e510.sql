SELECT cume_dist(ORDER BY i DESC) OVER w AS crash
FROM range(5_000) tbl(i)
WINDOW w AS (ORDER BY i ASC)
