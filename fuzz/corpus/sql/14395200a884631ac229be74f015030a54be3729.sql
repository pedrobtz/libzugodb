EXPLAIN SELECT (SELECT avg(inner_values.i) * 0.2
                FROM inner_values
                WHERE inner_values.i = outer_values.i
                HAVING true)
FROM outer_values;
