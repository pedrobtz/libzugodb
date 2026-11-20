SELECT i % 3 AS g, LAST(d), LAST(dt), LAST(t), LAST(s)
FROM five_dates
GROUP BY 1
ORDER BY 1
