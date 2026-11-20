CREATE TABLE customer AS SELECT i * 1000 AS ck, i % 25 AS nk FROM range(2000) t(i)
