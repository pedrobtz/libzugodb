SELECT * FROM test WHERE b=(SELECT MIN(b) FROM test AS a WHERE a.id=test.id AND a.id < test.b)
