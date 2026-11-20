SELECT finalize(list(r ORDER BY r % 2, r DESC) EXPORT_STATE) FROM range(5) t(r);
