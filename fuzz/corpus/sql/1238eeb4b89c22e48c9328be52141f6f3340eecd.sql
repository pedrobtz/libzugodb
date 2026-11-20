CREATE VIEW joined_walk AS
SELECT input.id, expanded.depth
FROM vertices input
CROSS JOIN LATERAL (
    WITH RECURSIVE walk(depth) AS (
        SELECT 0 + input.id * 0
        UNION ALL
        SELECT depth + 1 FROM walk WHERE depth < 7
    )
    SELECT depth FROM walk
) expanded
JOIN vertices target ON target.id = expanded.depth + 1;
