SELECT (1, 1) > ANY(SELECT * FROM (VALUES (0, 0), (2, 0), (1, 0)));
