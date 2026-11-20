SELECT first_value(value_col ORDER BY order_col IGNORE NULLS) over (PARTITION BY partition_col) 
FROM all_nulls;
