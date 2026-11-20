SELECT min(s.i), max(s.i) FROM read_parquet('{TEST_DIR}/large.parquet') WHERE s.i >= 500 AND s.i < 5000;
