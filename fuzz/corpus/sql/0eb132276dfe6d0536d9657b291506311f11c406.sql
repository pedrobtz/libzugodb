SELECT * FROM t2, t0 LEFT JOIN Lateral(SELECT t0.c0 AS col_0, t2.c0 AS col_1) as subQuery1 ON ((subQuery1.col_1)<(t0.c0));
