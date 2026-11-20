SELECT finalize(sum(DISTINCT x) EXPORT_STATE),
       finalize(count(DISTINCT x) EXPORT_STATE)
FROM distinct_rewrite_nulls;
