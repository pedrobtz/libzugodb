EXPLAIN
SELECT l.id, l.start, l.stop
FROM left_small l
ANTI JOIN right_small r
       ON l.start < r.stop
      AND r.start < l.stop
      AND l.symbol = r.symbol
      AND l.price + r.bid > 300
ORDER BY 1
