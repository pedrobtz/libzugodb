SELECT typeof((min(v) EXPORT_STATE)::BLOB) FROM (VALUES ('\xAA\xBB'::BLOB)) t(v);
