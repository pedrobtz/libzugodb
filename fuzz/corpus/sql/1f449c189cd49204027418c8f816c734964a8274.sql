SELECT approx_quantile(x, 0.5), finalize(to_aggregate_state((approx_quantile(x, 0.5) EXPORT_STATE)::VARCHAR::STRUCT(count UBIGINT, min DOUBLE, max DOUBLE, centroids STRUCT(mean DOUBLE, weight DOUBLE)[]), 'approx_quantile', ['DOUBLE', 'FLOAT'], [NULL, 0.5::FLOAT]))
FROM (VALUES (1.0::DOUBLE), ('inf'), ('-inf'), (2.0)) t(x);
