WITH t as (
    SELECT
        t1.col0 AS left_val,
        t2.col0 AS right_val,
    FROM
        (VALUES (0), (5), (10), (15)) AS t1
        ASOF JOIN (VALUES (1), (6), (11), (16)) AS t2
        ON t2.col0 > t1.col0
)
SELECT *
FROM t
WHERE right_val BETWEEN 3 AND 12
ORDER BY ALL
