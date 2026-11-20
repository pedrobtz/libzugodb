SELECT issue23927.s, sub0.s
FROM issue23927 
INNER JOIN issue23927 AS sub0 
ON (issue23927.s <= sub0.s) 
WHERE (issue23927.s < issue23927.s) IS NULL
ORDER BY ALL;
