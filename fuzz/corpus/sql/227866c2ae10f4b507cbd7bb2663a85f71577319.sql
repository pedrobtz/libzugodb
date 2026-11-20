SELECT i1.i,
	(SELECT SUM(s1.i)
	 FROM integers s1
	 WHERE s1.i = i1.i
	   AND s1.i = ANY (SELECT i2.i FROM integers i2 WHERE i2.i = s1.i))
FROM integers i1
ORDER BY i1.i;
