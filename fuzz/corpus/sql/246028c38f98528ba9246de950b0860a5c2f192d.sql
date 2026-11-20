SELECT i, (SELECT COUNT(*) FROM integers i2 WHERE i2.i>i1.i GROUP BY i1.i) FROM integers i1 ORDER BY i;
