SELECT count(*) FROM strings WHERE regexp_extract(s, '(missing)', 1, 'k') = s
