SELECT quantile_cont('2021-01-01'::TIMESTAMP + interval (r) second, 0.5) FROM quantile
