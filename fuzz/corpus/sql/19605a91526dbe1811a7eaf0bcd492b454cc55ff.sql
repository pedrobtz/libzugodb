SELECT finalize(combine(
    to_aggregate_state([{'v0': 1, 'v1': 10}, {'v0': 2, 'v1': 30}], 'list', ['INTEGER'], [NULL], [{'column': 1, 'order': 'ASC NULLS LAST'}]),
    to_aggregate_state([{'v0': 3, 'v1': 20}, {'v0': 4, 'v1': 40}], 'list', ['INTEGER'], [NULL], [{'column': 1, 'order': 'ASC NULLS LAST'}])
));
