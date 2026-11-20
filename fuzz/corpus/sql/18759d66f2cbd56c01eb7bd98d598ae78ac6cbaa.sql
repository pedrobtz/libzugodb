SELECT count(*) FROM ordered_state os JOIN (
    SELECT g, list(v ORDER BY v) AS direct FROM narrow GROUP BY g
) d USING (g)
WHERE finalize(os.l) IS DISTINCT FROM d.direct;
