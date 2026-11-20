SELECT u.n_name, COUNT(*) AS cnt
FROM (SELECT c.ck, n.n_name FROM nation n JOIN customer c ON n.nk = c.nk WHERE 1=0) u
JOIN orders o ON o.ck = u.ck
GROUP BY u.n_name ORDER BY u.n_name
