explain analyze SELECT * FROM left_table ANTI JOIN right_table ON ([left_table.a, left_table.b] = [right_table.a, right_table.b]);
