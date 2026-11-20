SELECT finalize(approx_quantile(i, 0.5) EXPORT_STATE) FROM (SELECT 1 AS i WHERE false) t;
