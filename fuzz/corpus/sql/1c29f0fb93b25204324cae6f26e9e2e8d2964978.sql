INSERT INTO sdb.fact SELECT i, 'K_' || (i % 5000) FROM range(20000) t(i)
