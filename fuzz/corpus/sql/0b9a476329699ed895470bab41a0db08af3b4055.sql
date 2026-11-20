SELECT * FROM test WHERE EXISTS(SELECT * FROM test2 WHERE test.a=test2.a AND test.b<>test2.c) order by 2;
