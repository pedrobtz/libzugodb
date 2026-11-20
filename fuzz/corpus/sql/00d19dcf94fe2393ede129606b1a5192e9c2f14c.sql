SELECT count(*) FROM fact f LEFT JOIN dim d ON f.k = d.k WHERE d.name IS NULL
