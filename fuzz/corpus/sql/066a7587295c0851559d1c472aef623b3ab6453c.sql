SELECT * FROM read_parquet('{TEST_DIR}/test_structs.parquet') WHERE i.a < 2;
