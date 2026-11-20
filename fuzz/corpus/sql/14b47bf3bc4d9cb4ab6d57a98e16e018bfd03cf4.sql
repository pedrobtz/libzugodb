SELECT 
    coalesce(anon_1.month, anon_2.month) AS month, 
    coalesce(coalesce(CAST(anon_1.value AS REAL), 0.0) + coalesce(CAST(anon_2.value AS REAL), 0.0), 0.0) AS value
FROM (
    SELECT coalesce(anon_3.month, anon_4.month) AS month, 
    coalesce(coalesce(CAST(anon_3.value AS REAL), 0.0) + coalesce(CAST(anon_4.value AS REAL), 0.0), 0.0) AS value
    FROM (
        SELECT month AS month, sum(anon_5.value) AS value
        FROM (
            SELECT date_trunc('month', day) AS month, coalesce(sum(value), 0.0) AS value
            FROM df1
            WHERE day >= CAST('2022-01-01 00:00:00' AS DATE) 
            AND day <= CAST('2022-01-31 00:00:00' AS DATE) 
            AND (organization ILIKE 'org1') 
            GROUP BY date_trunc('month', day)
        ) AS anon_5 
        GROUP BY GROUPING SETS((month))
    ) AS anon_3 
    FULL OUTER JOIN (
        SELECT month AS month, sum(anon_6.value) AS value
        FROM (
            SELECT date_trunc('month', day) AS month, coalesce(sum(value), 0.0) AS value
            FROM df2
            WHERE day >= CAST('2022-01-01 00:00:00' AS DATE) 
            AND day <= CAST('2022-01-31 00:00:00' AS DATE) 
            GROUP BY date_trunc('month', day)
        ) AS anon_6 
        GROUP BY GROUPING SETS((month))
    ) AS anon_4 ON anon_3.month = anon_4.month
) AS anon_1 
FULL OUTER JOIN (
    SELECT month AS month, sum(anon_7.value) AS value
    FROM (
        SELECT date_trunc('month', day) AS month, coalesce(sum(value), 0.0) AS value
        FROM df3
        WHERE day >= CAST('2022-01-01 00:00:00' AS DATE) 
        AND day <= CAST('2022-01-31 00:00:00' AS DATE) 
        GROUP BY date_trunc('month', day)
    ) AS anon_7 
    GROUP BY GROUPING SETS((month))
) AS anon_2 ON anon_1.month = anon_2.month
