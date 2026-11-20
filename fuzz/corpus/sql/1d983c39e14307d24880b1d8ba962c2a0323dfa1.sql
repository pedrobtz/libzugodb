SELECT l.a, l.x
FROM pair_l l
ANTI JOIN pair_r r ON EXISTS (
	SELECT 1 FROM pair_s s WHERE s.a = l.a AND s.b = r.b
) OR l.x * 10 = r.y
ORDER BY ALL;
