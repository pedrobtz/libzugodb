WITH d(v) AS (
	VALUES ('nan'::FLOAT), ('nan'::FLOAT), ('nan'::FLOAT), (1.0), (1.0)
)
SELECT mode(v)::VARCHAR, round(entropy(v), 6)::VARCHAR FROM d;
