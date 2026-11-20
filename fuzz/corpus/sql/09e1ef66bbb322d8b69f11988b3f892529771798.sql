SELECT i, f, SUM(i) OVER (ORDER BY i, f) FROM floats ORDER BY i, f
