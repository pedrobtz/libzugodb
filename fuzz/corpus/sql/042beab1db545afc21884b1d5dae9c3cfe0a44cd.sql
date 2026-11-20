SELECT x.col1, y.col1 FROM tbl_s_null x JOIN tbl_s_null y
ON x.col0 = y.col0 AND x.col1 > y.col1
ORDER BY x.col1.x, y.col1.x NULLS LAST;
