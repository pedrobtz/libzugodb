SELECT finalize(combine(
	(SELECT string_agg(d, '|') EXPORT_STATE FROM dummy WHERE d < 3),
	(SELECT string_agg(d, '-') EXPORT_STATE FROM dummy WHERE d >= 3 AND d < 6)))
