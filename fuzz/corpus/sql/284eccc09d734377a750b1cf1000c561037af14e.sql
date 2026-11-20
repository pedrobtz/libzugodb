SELECT finalize(to_aggregate_state(
    [{'v0': [3, 3], 'v1': 3}, {'v0': [1], 'v1': 1}, {'v0': [2], 'v1': 2}],
    'list', ['INTEGER[]'], [NULL], [{'column': 1, 'order': 'ASC NULLS LAST'}]
));
