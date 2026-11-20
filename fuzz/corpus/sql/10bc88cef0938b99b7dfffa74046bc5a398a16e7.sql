SELECT count(*) FROM u3 JOIN (
    SELECT x.k FROM (SELECT k, s FROM u1 WHERE s LIKE '%1%' OR s NOT LIKE '%1%') x
    JOIN (SELECT k, s FROM u2 WHERE s LIKE '%2%' OR s NOT LIKE '%2%') y USING (k)
) d USING (k)
