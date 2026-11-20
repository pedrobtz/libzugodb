SELECT count(w.value) FROM owners input, LATERAL (
    WITH RECURSIVE walk(value, depth) AS (
        SELECT nextval('body_calls') + coalesce(input.class, 0) * 0, 0
        UNION ALL SELECT value, depth + 1 FROM walk WHERE depth < 2
    ) SELECT * FROM walk
) w JOIN vertices target ON target.id = w.depth WHERE person = 1;
