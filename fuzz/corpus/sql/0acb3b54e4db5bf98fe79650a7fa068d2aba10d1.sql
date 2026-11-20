SELECT finalize(bool_and(v) EXPORT_STATE) FROM (VALUES (true), (false)) t(v)
