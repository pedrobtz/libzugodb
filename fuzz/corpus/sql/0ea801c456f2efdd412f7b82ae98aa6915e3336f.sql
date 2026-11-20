SELECT unique1 FROM tenk1 QUALIFY first_value(ten) OVER (PARTITION BY four ORDER BY ten) = 1 order by four, ten
