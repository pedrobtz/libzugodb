SELECT finalize(arg_min(a, b) EXPORT_STATE), finalize(arg_max(a, b) EXPORT_STATE) FROM (VALUES (1, 10), (2, 5), (3, 20)) t(a, b);
