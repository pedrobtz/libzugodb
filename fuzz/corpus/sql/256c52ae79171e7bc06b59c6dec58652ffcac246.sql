SELECT (SELECT CAST(CASE WHEN sum(inner_values.i) IS NULL THEN 'invalid' ELSE '1' END AS INTEGER)
        FROM inner_values
        WHERE inner_values.i = outer_values.i)
FROM outer_values
WHERE outer_values.i = 2;
