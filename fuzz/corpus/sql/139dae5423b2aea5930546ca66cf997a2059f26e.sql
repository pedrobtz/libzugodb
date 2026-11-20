WITH q AS (
	SELECT count(*)::INTEGER AS count_value
	FROM (SELECT a, nextval('pair_effect_seq') AS v FROM pair_effect_l) l
	FULL OUTER JOIN pair_effect_r r ON EXISTS (
		SELECT 1 WHERE l.v = r.b
	)
)
SELECT count_value, currval('pair_effect_seq') FROM q;
