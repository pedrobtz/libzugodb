CREATE TABLE retained AS
SELECT p, j, list(s) OVER w AS items, string_agg(s, '|') OVER w AS joined,
       row_number() OVER w AS rn, count(*) OVER w AS n
FROM cleanup_input
WINDOW w AS (PARTITION BY p ORDER BY j ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING)
ORDER BY p, j
