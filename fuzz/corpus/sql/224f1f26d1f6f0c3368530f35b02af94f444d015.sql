SELECT count(*) OVER w
FROM (VALUES (1), (2)) AS t(a)
WINDOW w AS (base), base AS (ORDER BY a);
