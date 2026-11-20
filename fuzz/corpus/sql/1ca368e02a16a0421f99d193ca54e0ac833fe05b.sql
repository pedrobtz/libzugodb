SELECT * FROM integers RIGHT JOIN (SELECT MAX(i) AS max_i FROM integers) ON i=max_i
