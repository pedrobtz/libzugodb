EXPLAIN SELECT (SELECT coalesce(avg(inner_values.i), 0)
                FROM inner_values
                WHERE inner_values.i = outer_values.i
                HAVING true)
FROM outer_values;
