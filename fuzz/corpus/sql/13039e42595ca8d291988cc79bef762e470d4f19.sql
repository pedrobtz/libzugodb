CREATE TABLE orders_small AS SELECT * FROM parquet_scan('{DATA_DIR}/parquet-testing/orders_small_parquet.test');
