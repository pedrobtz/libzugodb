SELECT g, finalize(arg_max(v, v ORDER BY v) EXPORT_STATE)
FROM (VALUES (0, 30), (0, 10), (0, 20), (1, 5), (1, 9)) t(g, v) GROUP BY g ORDER BY g;
