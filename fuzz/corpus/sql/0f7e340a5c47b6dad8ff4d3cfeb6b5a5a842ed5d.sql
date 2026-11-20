SELECT n.n_name, COUNT(*) AS cnt
FROM nation n JOIN customer c ON n.nk = c.nk JOIN orders_null_probe o ON o.ck = c.ck
GROUP BY n.n_name ORDER BY n.n_name
