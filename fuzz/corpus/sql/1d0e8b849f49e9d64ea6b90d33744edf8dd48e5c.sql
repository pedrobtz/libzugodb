SELECT f.fk, f.value, d.label
FROM fact f FULL OUTER JOIN dim d ON f.fk = d.id
WHERE d.id > 3 OR f.fk IS NULL
