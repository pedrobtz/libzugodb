SELECT f.fk, f.value, d.label
FROM fact f LEFT JOIN dim d ON f.fk = d.id
WHERE f.value = 777
