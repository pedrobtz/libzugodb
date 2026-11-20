SELECT r, substr(m, 1, 1) AS ch, length(m) AS len
FROM (SELECT r, mode(v) OVER (ORDER BY r ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) AS m FROM modes_str)
ORDER BY r;
