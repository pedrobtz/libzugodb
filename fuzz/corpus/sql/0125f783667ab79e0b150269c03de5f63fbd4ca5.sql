SELECT l.i, r.i, r.j
FROM non_inner_l l
FULL OUTER JOIN non_inner_r r ON l.i = (SELECT MIN(i) FROM non_inner_s)
ORDER BY l.i, r.i NULLS LAST, r.j NULLS LAST;
