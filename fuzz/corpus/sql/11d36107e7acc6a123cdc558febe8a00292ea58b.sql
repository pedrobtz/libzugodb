SELECT length(min(s)), length(max(s)) FROM (SELECT repeat(chr(CAST(97 + i % 26 AS INTEGER)), 1000 + i) AS s FROM range(60) t(i));
