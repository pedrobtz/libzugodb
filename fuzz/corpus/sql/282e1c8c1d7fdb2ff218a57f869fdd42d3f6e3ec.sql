SELECT cast(ASIN(n::double)*1000 as bigint) FROM numbers  WHERE n between -1 and 1 ORDER BY n
