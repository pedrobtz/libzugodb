SELECT (approx_top_k(x, 1) EXPORT_STATE)::STRUCT(k UBIGINT, "values" STRUCT("value" VARCHAR, "count" UBIGINT)[], "filter" UBIGINT[]).k FROM (VALUES ('a'), ('a'), ('b')) t(x);
