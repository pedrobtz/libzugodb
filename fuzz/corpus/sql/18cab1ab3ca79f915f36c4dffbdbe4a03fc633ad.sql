SELECT percentile_disc(.5) WITHIN GROUP (order by col desc) 
FROM VALUES (11000), (3100), (2900), (2800), (2600), (2500) AS tab(col);
