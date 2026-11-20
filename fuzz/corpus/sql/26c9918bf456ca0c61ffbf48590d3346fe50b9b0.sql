CREATE TABLE five_dates AS
	SELECT
		i::integer AS i,
		'2021-08-20'::DATE + i::INTEGER AS d,
		'2021-08-20'::TIMESTAMP + INTERVAL (i) HOUR AS dt,
		'14:59:37'::TIME + INTERVAL (i) MINUTE AS t,
		INTERVAL (i) SECOND AS s
	FROM range(1, 6, 1) t1(i)
