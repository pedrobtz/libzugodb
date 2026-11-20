SELECT quantile_disc('00:00:00'::TIME + interval (r) second, 0.5) FROM quantile
