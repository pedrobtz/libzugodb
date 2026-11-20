SELECT finalize(to_aggregate_state({
	'k': 2::UBIGINT,
	'values': [
		{'value': 'a'::VARCHAR, 'count': 0::UBIGINT}
	]::STRUCT("value" VARCHAR, "count" UBIGINT)[],
	'filter': list_transform(range(64), lambda x: 0::UBIGINT)
}, 'approx_top_k', ['VARCHAR', 'BIGINT']));
