SELECT d.name, count(*) FROM fact f JOIN dim d ON f.k = d.k GROUP BY d.name ORDER BY d.name
