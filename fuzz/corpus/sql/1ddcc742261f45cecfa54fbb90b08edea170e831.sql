SELECT i, i % 2 AS k, SUM(i) FROM integers GROUP BY i ORDER BY k, 3;
