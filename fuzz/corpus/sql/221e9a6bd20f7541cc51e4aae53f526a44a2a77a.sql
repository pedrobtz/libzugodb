SELECT finalize(combine(
    (SELECT list(s ORDER BY s) EXPORT_STATE FROM (VALUES ({'a': 1, 'b': 'x'}), ({'a': 3, 'b': 'z'})) t(s)),
    (SELECT list(s ORDER BY s) EXPORT_STATE FROM (VALUES ({'a': 2, 'b': 'y'}), ({'a': 4, 'b': 'w'})) t(s))
));
