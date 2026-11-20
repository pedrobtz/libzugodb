SELECT (arg_min(a, b) EXPORT_STATE)::STRUCT(arg INTEGER, "by" INTEGER) FROM (VALUES (1, 10), (2, 5), (3, 20)) t(a, b);
