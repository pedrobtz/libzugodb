CREATE MACRO array_rv(arr) AS (
    SELECT CASE WHEN l IS NOT NULL THEN l ELSE arr END 
    FROM (
        SELECT array_agg(elm ORDER BY g DESC) as l
        FROM (SELECT generate_subscripts(arr, 1) AS g, arr[g] AS elm)
    ) 
);
