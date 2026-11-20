SELECT finalize(list(i) EXPORT_STATE) FROM (VALUES (1), (NULL), (3)) t(i);
