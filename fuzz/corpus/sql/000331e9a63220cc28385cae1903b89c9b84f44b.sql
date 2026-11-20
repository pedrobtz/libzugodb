SELECT list_bool_and([*COLUMNS(*)]) FROM (SELECT finalize(min(COLUMNS(*)) EXPORT_STATE) = min(COLUMNS(*)) FROM test_all_types());
