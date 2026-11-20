SELECT * FROM t2, t1, ( SELECT t2.c0 AS col_1, t1.c0 AS col_2) as subQuery0 INNER JOIN t0 ON ((subQuery0.col_2)) CROSS JOIN (SELECT t0.c0 AS col_1);
