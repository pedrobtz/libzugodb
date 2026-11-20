WITH c AS MATERIALIZED (FROM mixed_dictionaries)
SELECT count(*), count(DISTINCT b.label)
FROM probe p JOIN (SELECT * FROM c ORDER BY k) b ON p.k=b.k
