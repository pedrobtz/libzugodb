SELECT count(*) FROM (SELECT approx_quantile(v, 0.99) AS q, approx_quantile(v, 0.5) AS q2 FROM t GROUP BY g) WHERE q >= q2;
