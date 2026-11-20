SELECT finalize(approx_top_k(x, 3) EXPORT_STATE) FROM (SELECT 'a' AS x WHERE false) t;
