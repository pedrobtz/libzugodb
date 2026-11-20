SELECT x.col1, y.col1 FROM tbl_l x JOIN tbl_l y
ON x.col0 = y.col0 AND x.col1 != y.col1
ORDER BY x.col1;
