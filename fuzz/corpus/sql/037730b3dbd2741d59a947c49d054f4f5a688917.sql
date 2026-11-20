SELECT
    i1.i AS i1_i,
    i2.s,
    i3.i AS i3_i
FROM
    integers1 i1
LEFT OUTER JOIN (integers2 i2 LEFT OUTER JOIN integers3 i3 ON i2.i = i3.i) on false;
