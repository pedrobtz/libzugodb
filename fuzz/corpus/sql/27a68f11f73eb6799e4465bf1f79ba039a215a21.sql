SELECT (1, 2) <> ANY (SELECT * FROM (VALUES (1, NULL::INT), (1, 2)) t(x, y))
