SELECT i IN (SELECT i1.i FROM (SELECT * FROM integers i1 WHERE i1.i=integers.i) i1 LEFT JOIN generate_series(1, 2, 1) tbl(i) ON i1.i=tbl.i) FROM integers ORDER BY i NULLS LAST;
