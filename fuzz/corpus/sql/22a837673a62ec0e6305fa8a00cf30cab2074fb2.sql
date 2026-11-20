SELECT finalize(combine(
    to_aggregate_state([{'v0': 'a', 'k': 2}, {'v0': NULL, 'k': 1}], 'count', ['ANY'], NULL,
                       [{'column': 1, 'order': 'DESC NULLS FIRST'}]),
    to_aggregate_state([{'v0': 'b', 'k': 3}], 'count', ['ANY'], NULL,
                       [{'column': 1, 'order': 'DESC NULLS FIRST'}])));
