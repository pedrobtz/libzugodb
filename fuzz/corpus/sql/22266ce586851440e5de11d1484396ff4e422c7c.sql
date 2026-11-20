SELECT DISTINCT ON (key1, key2) * FROM (VALUES(1,1,2),(1,1,3)) AS t(key1,key2,v) ORDER BY key1, key2
