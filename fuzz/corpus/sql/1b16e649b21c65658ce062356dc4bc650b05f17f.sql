EXPLAIN SELECT * FROM read_parquet('{TEST_DIR}/string_structs.parquet') WHERE s.a = 'foo';
