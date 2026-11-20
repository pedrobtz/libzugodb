EXPLAIN SELECT sum(tag), count_if(missing) FROM (SELECT id, depth, 1 AS tag, id IS NULL AS missing FROM walk_rows) q WHERE id = 3;
