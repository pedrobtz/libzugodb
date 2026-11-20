SELECT i, i % 2 AS i, SUM(i) FROM integers GROUP BY i ORDER BY i, 3;
