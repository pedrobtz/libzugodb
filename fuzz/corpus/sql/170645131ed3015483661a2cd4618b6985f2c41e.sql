SELECT * FROM outer_rows o, LATERAL (SELECT *) x;
