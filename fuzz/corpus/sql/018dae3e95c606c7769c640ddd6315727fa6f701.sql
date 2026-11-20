EXPLAIN (FORMAT JSON)
SELECT mode(v), count(DISTINCT ord) FROM mode_rewrite;
