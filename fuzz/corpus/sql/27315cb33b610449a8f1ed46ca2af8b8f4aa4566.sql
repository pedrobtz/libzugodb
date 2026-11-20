SELECT k, LEAST(v, 21) as c, SUM(v) FROM longlists
GROUP BY k, c
ORDER BY 2, 3
