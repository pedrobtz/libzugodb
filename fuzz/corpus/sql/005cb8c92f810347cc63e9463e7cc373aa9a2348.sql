SELECT approx_quantile('-infinity'::TIMESTAMP, 0.25), approx_quantile('infinity'::TIMESTAMP, 0.25)
