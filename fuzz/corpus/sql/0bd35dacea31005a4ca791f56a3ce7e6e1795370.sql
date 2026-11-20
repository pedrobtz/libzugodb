SELECT (arg_min_null(a, b) EXPORT_STATE)::STRUCT(arg INTEGER, "by" INTEGER) FROM (VALUES (NULL, 5), (2, 10)) t(a, b);
