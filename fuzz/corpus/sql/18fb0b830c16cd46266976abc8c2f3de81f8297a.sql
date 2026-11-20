SELECT 
    percentile_disc(0.8) WITHIN GROUP (ORDER BY x DESC),
    quantile_disc(x, 0.8 ORDER BY x DESC),
FROM 
    (VALUES (2), (1)) _(x);
