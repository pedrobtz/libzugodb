SELECT count(*), count(w.depth) FROM owners input LEFT JOIN LATERAL (
    WITH RECURSIVE walk(depth) AS (
        SELECT 0 + coalesce(input.class, 0) * 0
        UNION ALL SELECT depth + 1 FROM walk WHERE depth < 2
    ) SELECT * FROM walk WHERE depth > 2
) w ON true JOIN vertices target ON target.id = input.person WHERE person = 1;
