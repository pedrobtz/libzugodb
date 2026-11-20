CREATE TABLE nation_tiny AS SELECT i AS nk, (i % 5)::TINYINT AS n_t FROM range(25) t(i)
