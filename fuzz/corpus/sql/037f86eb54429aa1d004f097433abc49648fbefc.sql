select sum(distinct value), count(*), course, avg(distinct value), type
	from students
	group by grouping sets (course), grouping sets(type)
	order by all;
