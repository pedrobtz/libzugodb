SELECT i % 2 AS k, SUM(i) FROM integers GROUP BY k HAVING SUM(i) > (SELECT MAX(i) FROM integers)
