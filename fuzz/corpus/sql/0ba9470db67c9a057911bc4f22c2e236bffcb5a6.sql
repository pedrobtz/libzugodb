CREATE TABLE exact_prf_probe AS
SELECT (1000000000000 + i)::BIGINT AS k, (i % 7)::INTEGER AS g
FROM range(100000) t(i);
