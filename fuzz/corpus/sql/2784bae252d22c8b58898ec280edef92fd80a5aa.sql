INSERT INTO cdb.fact_num SELECT (i // 122880) * 2000000 FROM range(245760) t(i)
