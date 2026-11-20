SELECT test.b, SUM(a) FROM test GROUP BY test.b HAVING SUM(a)=(SELECT SUM(a) FROM test t WHERE test.b=t.b) ORDER BY test.b;
