WITH t1 AS (
	FROM VALUES
		(1::INT, '2020-01-01 00:00:00'::TIMESTAMP), 
		(2, '2020-01-02 00:00:00')
	AS t1(a, b)
), t2 AS (
	FROM VALUES
		(1::INT, '2020-01-01 00:01:00'::TIMESTAMP), 
		(2, '2020-01-02 00:00:00')
	t2(c, d)
)
SELECT * 
FROM t1 
ASOF JOIN t2 
  ON t1 = (b == t2.d)
  AND t1.b >= t2.d - INTERVAL '1' SECOND;
