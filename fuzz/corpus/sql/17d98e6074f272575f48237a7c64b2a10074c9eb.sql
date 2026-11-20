SELECT COALESCE(u.n_name, 'NO_BUILD') AS nm, COUNT(*) AS cnt
FROM orders_miss o
RIGHT JOIN (SELECT c.ck, n.n_name FROM nation n JOIN customer_orphan c ON n.nk = c.nk) u ON o.ck = u.ck
GROUP BY nm ORDER BY nm
