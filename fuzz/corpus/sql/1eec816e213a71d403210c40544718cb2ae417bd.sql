WITH cte AS (
    SELECT NULL::INTERVAL i,
           NULL::DATE d,
           NULL::TIMESTAMP t
    FROM range(1)
)
SELECT time_bucket(i, d)
FROM cte
