INSERT INTO mem.fact_i SELECT (i % 3) + 1 FROM range(300) t(i)
