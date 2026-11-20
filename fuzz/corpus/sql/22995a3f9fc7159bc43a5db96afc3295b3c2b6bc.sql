SELECT finalize(
	to_aggregate_state(
	{
		'k': CAST(1099511627776 AS UBIGINT),
		'values': list_value({'value': 7, 'count': CAST(5 AS UBIGINT)}),
		'filter': list_value(CAST(0 AS UBIGINT), CAST(0 AS UBIGINT))
	},
	'approx_top_k', ['INTEGER', 'BIGINT'], [NULL, 3]
	)
);
