SELECT x, MAX(x) OVER none FROM (VALUES (1)) t(x);
