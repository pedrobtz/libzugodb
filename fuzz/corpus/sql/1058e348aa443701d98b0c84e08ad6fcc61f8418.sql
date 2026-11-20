CREATE VIEW v1 AS
SELECT i, j, k, l FROM integers FULL OUTER JOIN integers2 ON integers.i=integers2.k
UNION ALL
SELECT i, j, k, l FROM integers FULL OUTER JOIN integers2 ON integers.i=integers2.k
