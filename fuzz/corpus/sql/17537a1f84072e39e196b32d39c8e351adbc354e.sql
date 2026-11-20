PREPARE pair_prepared_full AS
SELECT l.a, r.b
FROM (SELECT * FROM pair_l WHERE x >= $1) l
FULL OUTER JOIN pair_r r ON EXISTS (
	SELECT 1
	FROM pair_s s
	WHERE s.a = l.a
	  AND s.b = r.b
	  AND s.a <= $2
)
ORDER BY l.a NULLS LAST, r.b NULLS LAST;
