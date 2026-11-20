SELECT quantile_disc(col, [0.1, 0.32, 0.33, 0.34, 0.49, .5, .51, 0.75, 0.9, 0.999, 1])
FROM VALUES (0), (1), (2), (10) AS tab(col);
