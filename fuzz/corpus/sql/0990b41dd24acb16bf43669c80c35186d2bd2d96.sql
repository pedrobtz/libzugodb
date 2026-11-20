SELECT s.precision, len(s.registers), list_max(s.registers) BETWEEN 1 AND 55 FROM (
    SELECT (approx_count_distinct(i) EXPORT_STATE)::STRUCT(precision UTINYINT, registers UTINYINT[]) s
    FROM range(1000) t(i)
) t;
