SELECT finalize(min({'a': v, 'b': v::varchar}) EXPORT_STATE) FROM (VALUES (3), (1), (5)) t(v);
