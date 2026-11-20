SELECT b, SUM(a) FROM test GROUP BY b QUALIFY row_number() OVER (PARTITION BY b) > sum;
