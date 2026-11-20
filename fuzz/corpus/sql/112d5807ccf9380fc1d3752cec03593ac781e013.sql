SELECT name, total FROM students JOIN LATERAL (SELECT SUM(grade) AS total FROM exams WHERE exams.sid=students.id) grades ON true ORDER BY total DESC;
