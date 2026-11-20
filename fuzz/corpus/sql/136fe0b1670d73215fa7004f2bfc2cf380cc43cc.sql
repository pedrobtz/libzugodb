CREATE TABLE three AS 
	SELECT * FROM (VALUES
		(11, 1),
		(12, 2),
		(13, 3)
	) tbl(a, b);
