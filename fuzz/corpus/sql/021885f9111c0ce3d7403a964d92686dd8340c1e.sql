EXPLAIN SELECT sum(v) FROM (SELECT id, __internal_barrier(id) AS v FROM walk_rows) q WHERE id = 3;
