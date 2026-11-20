SELECT cast(COT(n::integer)*1000 as bigint) FROM numbers  WHERE n > 0.1 OR N < -0.1 ORDER BY n
