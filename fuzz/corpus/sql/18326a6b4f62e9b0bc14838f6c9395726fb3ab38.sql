SELECT a % 2, b FROM test UNION SELECT a % 2 AS k, b FROM test ORDER BY -1
