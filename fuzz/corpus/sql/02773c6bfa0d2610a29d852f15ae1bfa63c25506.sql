EXPLAIN SELECT regexp_replace(s, '^(foo)-(.*)$', '\2', 'i')
FROM (VALUES ('FOO-bar')) t(s);
