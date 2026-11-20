CREATE {storage_type} intlists AS SELECT * FROM (VALUES
	(21, [1]),
	(22, [NULL]),
	(23, []),
	(24, [2, 3]),
	(NULL::INTEGER, [13]),
	(32, [NULL]),
	(34, [2, 3])
	) lv(v, k);
