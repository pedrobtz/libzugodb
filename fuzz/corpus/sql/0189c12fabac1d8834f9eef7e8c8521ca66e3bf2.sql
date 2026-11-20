EXPLAIN ANALYZE
SELECT count(*)
FROM probe_timestamp p
JOIN build_timestamp b
  ON p.k = b.k
 AND p.g >= b.g;
