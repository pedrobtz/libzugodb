SELECT regexp_matches(s, E'a\n')
FROM (VALUES ('a'), (E'a\n')) values_table(s)
ORDER BY length(s)
