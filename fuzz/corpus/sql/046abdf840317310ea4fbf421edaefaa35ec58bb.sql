create or replace table tleft as
	select 
		i as k100,
		'2024-01-01'::TIMESTAMP + INTERVAL (i) seconds as b100,
		b100 + INTERVAL 1 second as e100,
	from range(10) as tbl(i);
