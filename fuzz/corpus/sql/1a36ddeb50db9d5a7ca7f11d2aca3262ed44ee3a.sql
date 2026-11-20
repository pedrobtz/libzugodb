EXPLAIN SELECT * FROM read_parquet('{TEST_DIR}/test_structs.parquet') WHERE i.b = true or i.a IS NULL;
