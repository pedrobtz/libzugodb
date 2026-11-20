SELECT finalize(s), list_max(s::STRUCT(precision UTINYINT, registers UTINYINT[]).registers) FROM (
    SELECT approx_count_distinct(x) EXPORT_STATE s FROM (SELECT NULL::INTEGER AS x FROM range(10))
) t;
