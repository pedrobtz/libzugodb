SELECT x.col1, y.col1 FROM tbl x JOIN tbl y
ON x.col0 = y.col0 AND (x.col1 IS NOT DISTINCT FROM y.col1)
ORDER BY x.col1;
