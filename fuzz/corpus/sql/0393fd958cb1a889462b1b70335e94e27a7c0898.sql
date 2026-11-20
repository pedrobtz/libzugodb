INSERT INTO prf_probe_string_nul VALUES
	(1, 'ab' || chr(0) || 'd', 2),
	(2, 'ab' || chr(0) || 'x', 3),
	(3, 'ab', 4),
	(4, 'zzzz', 5);
