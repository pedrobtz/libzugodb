SELECT finalize(to_aggregate_state(
    [{'v0': 'b', 'v1': 2}, {'v0': 'a', 'v1': 1}, {'v0': 'c', 'v1': 3}],
    'string_agg', ['VARCHAR', 'VARCHAR'], [NULL, ' '],
    [{'column': 1, 'order': 'ASC NULLS LAST'}]
));
