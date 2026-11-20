explain
SELECT tt1.i, tt2.k
   FROM tt1
   ASOF JOIN tt2 ON
  tt1.j = tt2.j AND tt1.i >= tt2.i
  ORDER BY tt1.i;
