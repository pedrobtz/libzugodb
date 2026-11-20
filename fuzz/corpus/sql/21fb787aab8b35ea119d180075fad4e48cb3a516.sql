INSERT INTO distinct_on_test VALUES
	(1, 'hello', ARRAY[1], 42), -- ASC
	(1, 'hello', ARRAY[1], 42),
	(1, 'hello', ARRAY[1], 43), -- DESC
	(2, NULL, NULL, 0),     -- ASC
	(2, NULL, NULL, 1),
	(2, NULL, NULL, NULL),  -- DESC
	(3, 'thisisalongstring', NULL, 0),     -- ASC
	(3, 'thisisalongstringbutlonger', NULL, 1),
	(3, 'thisisalongstringbutevenlonger', ARRAY[1, 2, 3, 4, 5, 6, 7, 8, 9], 2)  -- DESC
;
