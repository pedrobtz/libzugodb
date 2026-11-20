CREATE TABLE state_tbl AS SELECT g, string_agg(d, '-') EXPORT_STATE s FROM dummy GROUP BY g;
