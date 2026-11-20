SELECT id, (
    WITH a AS MATERIALIZED (SELECT unnest(v.xs) AS x),
         b AS MATERIALIZED (SELECT unnest(v.ys) AS y)
    SELECT [list(x IN (SELECT y FROM b) ORDER BY x NULLS LAST),
            list(x NOT IN (SELECT y FROM b) ORDER BY x NULLS LAST)]
    FROM a
) AS found
FROM inputs v ORDER BY id;
