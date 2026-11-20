SELECT d.name, count(f.id) FROM fact f RIGHT JOIN dim d ON f.k = d.k GROUP BY d.name ORDER BY d.name
