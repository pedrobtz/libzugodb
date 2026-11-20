SELECT n.n_name, COUNT(*) AS cnt
FROM nation n
JOIN customer c ON n.nk = c.nk
JOIN orders_c o ON o.ck = c.ck
JOIN lineitem l ON l.ok = o.ok
GROUP BY n.n_name ORDER BY n.n_name
