SELECT count(*) FROM (
    SELECT g, list(v ORDER BY v DESC) AS direct FROM wide GROUP BY g
) d JOIN (
    SELECT g, finalize(list(v ORDER BY v DESC) EXPORT_STATE) AS exported FROM wide GROUP BY g
) e USING (g)
WHERE d.direct IS DISTINCT FROM e.exported;
