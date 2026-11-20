CREATE TABLE customer4 AS SELECT i * 1000 AS ck, i % 70000 AS nk FROM range(200000) t(i)
