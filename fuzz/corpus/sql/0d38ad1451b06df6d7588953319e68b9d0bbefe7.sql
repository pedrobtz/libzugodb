SELECT i, LEAD(i, 1) OVER() AS i1
FROM range(10) tbl(i);
