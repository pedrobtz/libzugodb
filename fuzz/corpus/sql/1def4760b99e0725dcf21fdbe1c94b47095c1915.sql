SELECT i, (SELECT COUNT(i) FROM integers i2 WHERE i2.i>i1.i OR i2.i IS NULL) FROM integers i1 ORDER BY i;
