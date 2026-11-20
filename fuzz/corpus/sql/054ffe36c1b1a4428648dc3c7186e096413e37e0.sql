SELECT finalize(approx_quantile(i, [0.1, 0.5, 0.9]) EXPORT_STATE) = approx_quantile(i, [0.1, 0.5, 0.9]) FROM range(1, 10001) t(i);
