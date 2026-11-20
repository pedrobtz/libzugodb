SELECT * FROM (SELECT * FROM window_table ORDER BY rowid) EXCEPT SELECT * FROM (SELECT * FROM agg_table ORDER BY rowid);
