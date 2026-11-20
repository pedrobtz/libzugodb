WITH left_side AS (
	SELECT l.id AS lid, l.a, l2.z
	FROM pair_multi_both_l l
	JOIN pair_multi_both_l2 l2 ON l.id = l2.lid
), right_side AS (
	SELECT r.id AS rid, r.b, r2.w
	FROM pair_multi_both_r r
	JOIN pair_multi_both_r2 r2 ON r.id = r2.rid
), actual AS (
	SELECT l.id AS lid, l.a, l2.z
	FROM pair_multi_both_l l
	JOIN pair_multi_both_l2 l2 ON l.id = l2.lid
	SEMI JOIN (
		pair_multi_both_r r
		JOIN pair_multi_both_r2 r2 ON r.id = r2.rid
	) ON EXISTS (
		SELECT 1
		FROM pair_multi_both_s s
		WHERE s.a IS NOT DISTINCT FROM l.a
		  AND s.b IS NOT DISTINCT FROM r.b
		  AND s.z IS NOT DISTINCT FROM l2.z
		  AND s.w IS NOT DISTINCT FROM r2.w
	)
), manual AS (
	SELECT l.lid, l.a, l.z
	FROM left_side l
	WHERE EXISTS (
		SELECT 1
		FROM right_side r
		WHERE EXISTS (
			SELECT 1
			FROM pair_multi_both_s s
			WHERE s.a IS NOT DISTINCT FROM l.a
			  AND s.b IS NOT DISTINCT FROM r.b
			  AND s.z IS NOT DISTINCT FROM l.z
			  AND s.w IS NOT DISTINCT FROM r.w
		)
	)
), a_minus_m AS (
	SELECT * FROM actual EXCEPT ALL SELECT * FROM manual
), m_minus_a AS (
	SELECT * FROM manual EXCEPT ALL SELECT * FROM actual
)
SELECT 'semi_multi_layout', (SELECT count(*) FROM a_minus_m), (SELECT count(*) FROM m_minus_a);
