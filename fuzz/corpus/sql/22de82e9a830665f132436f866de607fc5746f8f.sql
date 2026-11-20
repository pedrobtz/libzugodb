WITH states AS (
    SELECT sum(v) EXPORT_STATE AS sum_s, count(v) EXPORT_STATE AS count_s
    FROM (VALUES (1), (3), (5)) t(v)
)
SELECT
    (finalize(combine_aggr(sum_s, 0)) IS NULL)::INTEGER,
    finalize(combine_aggr(count_s, NULL::BIGINT))
FROM states;
