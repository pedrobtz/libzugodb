SELECT quantile_cont('00:00:00'::TIME + interval (r/100) second, [0.25, 0.5, 0.75]) FROM quantiles
