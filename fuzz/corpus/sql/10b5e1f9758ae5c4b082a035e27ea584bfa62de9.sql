SELECT l.o,
       r.wh,
       r.wh <> s2.wh AS direct_comparison,
       EXISTS (
           SELECT 1
           FROM issue23979_s2 x
           WHERE x.o = l.o
             AND r.wh <> x.wh
       ) AS correlated_exists,
       EXISTS (
           SELECT 1
           FROM issue23979_s2 x
           WHERE x.o = l.o
             AND (r.wh + 0) <> x.wh
       ) AS equivalent_rewrite
FROM issue23979_l l
LEFT JOIN issue23979_r r USING (o)
JOIN issue23979_s2 s2 USING (o)
ORDER BY l.o;
