SELECT finalize(to_aggregate_state([{'v0': 1::DOUBLE, 'v1': '10'}, {'v0': 2::DOUBLE, 'v1': '20'}], 'arg_max', ['INTEGER', 'INTEGER'], NULL, [{'column': 0, 'order': 'ASC NULLS FIRST'}]));
