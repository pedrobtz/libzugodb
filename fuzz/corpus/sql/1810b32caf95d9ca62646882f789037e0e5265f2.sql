SELECT (arg_min(a, b) EXPORT_STATE)::STRUCT(arg INTEGER[], "by" INTEGER[]) FROM (VALUES ([1, 2], [10]), ([3], [5])) t(a, b);
