SELECT i, j, k, l FROM integers FULL OUTER JOIN integers2 ON integers.i=integers2.k
UNION
SELECT i, j, k, l FROM integers FULL OUTER JOIN integers2 ON integers.i=integers2.k
ORDER BY i
