select course, type, count(*) from students group by cube (course), cube (type) order by 1, 2, 3;
