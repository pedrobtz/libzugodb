SELECT first(DISTINCT i ORDER BY random() * i)
FROM (VALUES 
	(900),
	(2600),
	(2100),
	(2100),
	(1800)
	) AS t(i);
