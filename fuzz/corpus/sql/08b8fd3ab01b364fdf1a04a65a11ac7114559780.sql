SELECT x.col1, y.col1 FROM tbl_s x JOIN tbl_s y
ON x.col0 = y.col0 AND (x.col1 IS DISTINCT FROM y.col1)
ORDER BY x.col1;
