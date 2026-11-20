SELECT QUANTILE_DISC(ts, [0.0, 0.5, 1.0]), QUANTILE_DISC(tstz, [0.0, 0.5, 1.0]), QUANTILE_DISC(dt, [0.0, 0.5, 1.0])
FROM specials;
