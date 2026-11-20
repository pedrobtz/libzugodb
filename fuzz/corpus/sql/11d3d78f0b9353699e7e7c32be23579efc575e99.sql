SELECT percentile_disc(quantile := 0.25) WITHIN GROUP (ORDER BY i DESC) FROM generate_series(0,100) tbl(i);
