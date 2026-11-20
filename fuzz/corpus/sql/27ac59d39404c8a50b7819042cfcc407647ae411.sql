SELECT *, (SELECT MAX(i) FROM integers) FROM integers ORDER BY i
