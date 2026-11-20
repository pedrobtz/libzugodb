CREATE {storage_type} structs AS SELECT * FROM (VALUES
	(21, {'x': 1, 'y': 'a'}),
	(22, {'x': NULL, 'y': NULL}),
	(23, {'x': 0, 'y': ''}),
	(24, {'x': 2, 'y': 'c'}),
	(NULL::INTEGER, {'x': 13, 'y': 'Somateria mollissima'}),
	(32, {'x': NULL, 'y': NULL}),
	(34, {'x': 2, 'y': 'c'})
	) sv(v, k);
