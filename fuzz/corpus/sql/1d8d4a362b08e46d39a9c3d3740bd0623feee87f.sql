SELECT outer_values.i,
       (SELECT 7 FROM inner_values WHERE inner_values.i = outer_values.i HAVING sum(inner_values.i) IS NULL),
       (SELECT outer_values.i FROM inner_values WHERE inner_values.i = outer_values.i HAVING true),
       (SELECT sum(inner_values.i) FROM inner_values WHERE inner_values.i = outer_values.i),
       (SELECT coalesce(sum(inner_values.i), 42)
        FROM inner_values WHERE inner_values.i = outer_values.i HAVING true),
       (SELECT sum(inner_values.i) IS NULL
        FROM inner_values WHERE inner_values.i = outer_values.i HAVING true)
FROM outer_values
ORDER BY outer_values.i;
