SELECT count(*) FILTER (WHERE filled IS NULL)
FROM (
    SELECT fill(CASE WHEN i = 2 THEN NULL ELSE i END) OVER (ORDER BY i) AS filled
    FROM (SELECT i::DOUBLE AS i FROM range(5) r(i) UNION ALL SELECT 'infinity'::DOUBLE) t
);
