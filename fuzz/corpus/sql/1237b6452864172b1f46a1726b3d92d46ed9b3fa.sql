SELECT a, (SELECT SUM(b) FROM test tsub WHERE test.a=tsub.a) FROM test
