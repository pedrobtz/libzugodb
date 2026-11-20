SELECT sum(distinct value), GROUPING(course, type), course, type, COUNT(*), sum(distinct value), FROM students GROUP BY CUBE(course, type) ORDER BY all;
