WITH t AS (
  SELECT 1 AS r, ARRAY[1, 2, 3] AS a
  UNION SELECT 2 AS r, ARRAY[4] AS a
  UNION SELECT 4 AS r, ARRAY[] AS a
)
SELECT r, a.value
FROM t
LEFT JOIN UNNEST(a) AS a(value) ON (1 = 1) AND TRUE AND list_contains([2, 3], 2)
ORDER BY 1, 2
