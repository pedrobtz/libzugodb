SELECT finalize(list(i) EXPORT_STATE) FROM (VALUES (1), (2), (3)) t(i);
