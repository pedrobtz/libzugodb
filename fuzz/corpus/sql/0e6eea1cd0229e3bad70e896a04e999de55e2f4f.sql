SELECT finalize(to_aggregate_state([1, NULL]::BIGINT[], 'median', ['BIGINT'], NULL));
