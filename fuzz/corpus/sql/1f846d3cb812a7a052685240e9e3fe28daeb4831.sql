INSERT INTO paths VALUES
	(1, 'a/b\c', 'forward_slash'),
	(2, 'a/b\c', 'backslash'),
	(3, 'a/b\c', NULL),
	(4, 'a/b\c', 'both_slash'),
	(5, '', 'backslash'),
	(6, NULL, 'forward_slash'),
	(7, 'a/b\c', ''),
	(8, 'a/b\c', '@');
