SELECT count(*) FROM range(5003) WHERE nextval('null_selection_seq') IS NOT NULL;
