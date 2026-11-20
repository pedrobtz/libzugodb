select count(*) from a, (SELECT 1 AS j) b where i > j
