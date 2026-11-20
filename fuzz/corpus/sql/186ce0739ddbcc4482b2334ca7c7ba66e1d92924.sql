select sum(distinct value), count(*), course, type
	from students
	group by grouping sets((course, type), (course))
	order by all;
