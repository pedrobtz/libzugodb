SELECT count(*) FILTER (WHERE result), currval('correlated_effect_seq')
FROM (
	SELECT EXISTS (
		SELECT 1
		WHERE nextval('correlated_effect_seq') = 2
		  AND outer_values.a = 1
	) AS result
	FROM (VALUES (1, 1), (2, 1)) outer_values(id, a)
);
