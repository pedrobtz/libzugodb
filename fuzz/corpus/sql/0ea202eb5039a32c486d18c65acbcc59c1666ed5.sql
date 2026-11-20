EXPLAIN ANALYZE
SELECT count(*)
FROM prf_string_nul_stats.prf_probe_string_nul_stats p
JOIN prf_build_string_nul_stats b
  ON p.k = b.k
 AND p.g >= b.g;
