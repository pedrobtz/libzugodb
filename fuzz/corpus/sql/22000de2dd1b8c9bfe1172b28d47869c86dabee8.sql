SELECT finalize(to_aggregate_state({'k': 3, 'values': [{'value': 5, 'count': 1}], 'filter': list_resize([NULL]::UBIGINT[], 128, NULL)}, 'approx_top_k', ['INTEGER', 'BIGINT']));
