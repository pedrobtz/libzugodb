SELECT count(*) FROM fact f JOIN dim d ON f.k IS NOT DISTINCT FROM d.k
