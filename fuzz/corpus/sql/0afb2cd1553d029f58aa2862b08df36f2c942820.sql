EXPLAIN SELECT count(*) FROM (SELECT id + 1 AS next_id FROM walk_rows) q WHERE next_id = 4;
