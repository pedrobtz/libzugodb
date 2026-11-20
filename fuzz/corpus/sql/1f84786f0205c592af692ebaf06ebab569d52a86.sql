select course, type, count(*) from students group by cube (course, type, course) order by 1, 2, 3;
