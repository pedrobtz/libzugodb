SELECT finalize(list(a) EXPORT_STATE) FROM (VALUES ([1, 2, 3]::INTEGER[3]), ([4, 5, 6]::INTEGER[3])) t(a);
