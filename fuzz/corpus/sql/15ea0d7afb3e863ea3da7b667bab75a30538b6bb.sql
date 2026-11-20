SELECT l IN (SELECT i1.l FROM (SELECT * FROM lists i1 WHERE i1.l=lists.l) i1 LEFT JOIN generate_series(1, 2, 1) tbl(s) ON i1.l=ARRAY[tbl.s]) FROM lists ORDER BY l NULLS LAST;
