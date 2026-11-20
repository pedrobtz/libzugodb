SELECT k, LEAST(v, 21) as c, SUM(v) FROM structs GROUP BY k, c ORDER BY 2, 3
