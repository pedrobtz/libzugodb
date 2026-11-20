SELECT g, finalize(skewness(d) EXPORT_STATE) FROM dummy GROUP BY g ORDER BY g;
