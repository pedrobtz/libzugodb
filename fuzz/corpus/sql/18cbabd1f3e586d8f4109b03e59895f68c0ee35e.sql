SELECT column_name, column_default, data_type
FROM information_schema.columns
WHERE table_name = 't_kw_udt'
ORDER BY column_name;
