WITH nullable_counts AS (
    SELECT g, count(v) EXPORT_STATE c, count(s) EXPORT_STATE t
    FROM (VALUES (0,1::BIGINT,'a'),(0,NULL,NULL),(0,1,'a'),(1,NULL,NULL),(2,2,'b')) input(g,v,s)
    GROUP BY g
)
SELECT g, finalize(c), finalize(combine(c, to_aggregate_state(t::BIGINT, 'count', ['ANY'])))
FROM nullable_counts;
