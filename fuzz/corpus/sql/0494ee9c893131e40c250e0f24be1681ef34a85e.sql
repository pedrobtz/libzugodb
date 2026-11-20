SELECT finalize(combine_aggr(s)) FROM (
    SELECT min(v) EXPORT_STATE AS s FROM (VALUES ('banana')) t(v)
    UNION ALL
    SELECT min(v) EXPORT_STATE AS s FROM (SELECT NULL::varchar AS v WHERE false) t
) t;
