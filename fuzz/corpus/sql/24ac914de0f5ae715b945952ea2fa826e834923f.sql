SELECT GROUPING(course, type, course, course, type, type, course), course, type, COUNT(*) FROM students GROUP BY CUBE(course, type) ORDER BY 1, 2, 3, 4;
