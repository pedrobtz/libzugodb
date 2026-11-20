SELECT a, SUM(a), (SELECT SUM(a)+SUM(t1.b) FROM test) FROM test t1 GROUP BY a ORDER BY a;
