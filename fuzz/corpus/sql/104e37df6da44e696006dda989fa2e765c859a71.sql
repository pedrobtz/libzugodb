SELECT g, finalize(first(v ORDER BY v) EXPORT_STATE), finalize(last(v ORDER BY v) EXPORT_STATE),
       finalize(any_value(v ORDER BY v DESC) EXPORT_STATE)
FROM (VALUES (0, 30), (0, 10), (0, 20), (1, 5), (1, 9)) t(g, v) GROUP BY g ORDER BY g;
