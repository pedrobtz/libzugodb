FROM (VALUES (1,1,2),(1,1,3),(2,1,4)) AS t(key1,key2,v)
SELECT DISTINCT ON (COLUMNS('key'), v) key1, key2, v ORDER BY key1, v
