SELECT GROUPING(course), GROUPING(value), course, sum(distinct value), COUNT(*) FROM students GROUP BY CUBE(course, value) HAVING GROUPING(course)=0 ORDER BY all;
