EXPLAIN SELECT count(*) FROM (
    SELECT input.person, NOT EXISTS (
        WITH RECURSIVE walk(depth) AS (
            SELECT coalesce(input.class,0)*0
            UNION ALL
            SELECT depth+1 FROM walk WHERE depth<2
        ) SELECT * FROM walk WHERE depth=2 AND input.class IS NOT NULL) AS found
    FROM owners input JOIN vertices target ON target.id=input.person
) q WHERE person=1 AND found;
