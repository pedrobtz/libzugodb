WITH d(v) AS (
	VALUES ('nan'::DOUBLE), ('nan'::DOUBLE), ('nan'::DOUBLE), (1.0), (1.0)
)
SELECT mode(v) OVER ()::VARCHAR, round(entropy(v) OVER (), 6)::VARCHAR
FROM d
LIMIT 1;
