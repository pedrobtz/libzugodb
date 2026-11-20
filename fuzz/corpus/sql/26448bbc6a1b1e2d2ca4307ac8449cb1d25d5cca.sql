SELECT (i = 1 OR i = 9999) AS nan_group, MIN(f), MAX(f) FROM fl GROUP BY 1 ORDER BY 1
