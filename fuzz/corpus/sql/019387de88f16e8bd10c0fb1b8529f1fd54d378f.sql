SELECT * FROM left_table l
SEMI JOIN right_table r ON l.id = r.id AND l.val > 1;
