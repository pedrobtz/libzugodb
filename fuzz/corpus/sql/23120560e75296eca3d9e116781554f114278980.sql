CREATE TABLE cleanup_input AS
SELECT p, j, lpad((p * 521 + j)::VARCHAR, 20, '0') AS s
FROM range(257) parts(p), range(521) rows(j)
