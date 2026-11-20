SELECT DISTINCT ON (COLUMNS('a'), COLUMNS('b')) * FROM complex_table ORDER BY a, b, c
