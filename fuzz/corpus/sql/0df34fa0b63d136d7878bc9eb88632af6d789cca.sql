SELECT (SELECT test.a+test.b+SUM(test2.a) FROM test2 WHERE str=str2) FROM test ORDER BY 1;
