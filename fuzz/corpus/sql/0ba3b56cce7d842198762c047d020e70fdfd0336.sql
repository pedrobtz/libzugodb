SELECT finalize(string_agg(d, '''' || chr(10) || '🦆') EXPORT_STATE) FROM dummy;
