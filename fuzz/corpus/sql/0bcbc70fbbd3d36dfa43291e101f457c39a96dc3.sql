SELECT GROUPING(), course, type, COUNT(*) FROM students GROUP BY CUBE(course, type) ORDER BY 1, 2, 3, 4;
