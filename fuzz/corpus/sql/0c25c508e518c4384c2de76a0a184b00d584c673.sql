WITH q AS (
	SELECT count(*)::INTEGER AS count_value
	FROM (SELECT a, nextval('pair_anti_effect_seq') AS v FROM pair_left_effect_l) l
	ANTI JOIN pair_left_effect_r r ON EXISTS (
		SELECT 1 WHERE l.v = r.b
	)
)
SELECT count_value, currval('pair_anti_effect_seq') FROM q;
