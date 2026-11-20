COPY (SELECT g, count(d) EXPORT_STATE s1 FROM dummy GROUP BY g) TO '{TEST_DIR}/count_state.parquet' (FORMAT PARQUET);
