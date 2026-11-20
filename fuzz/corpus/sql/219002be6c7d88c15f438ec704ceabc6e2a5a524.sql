INSERT INTO test SELECT a||a||a||a||a||a||a||a||a||a FROM test WHERE LENGTH(a)=(SELECT MAX(LENGTH(a)) FROM test)
