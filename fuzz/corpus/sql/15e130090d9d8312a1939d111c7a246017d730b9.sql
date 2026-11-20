WITH c AS (
	SELECT flag FROM ordered_probe ORDER BY ord LIMIT 1
)
SELECT * FROM c INNER JOIN boolean_keys ON flag = flag_key;
