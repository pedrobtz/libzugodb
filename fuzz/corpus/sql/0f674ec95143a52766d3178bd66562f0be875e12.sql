SELECT sum(distinct value), GROUPING_ID(course), course, COUNT(*) FROM students GROUP BY course ORDER BY all;
