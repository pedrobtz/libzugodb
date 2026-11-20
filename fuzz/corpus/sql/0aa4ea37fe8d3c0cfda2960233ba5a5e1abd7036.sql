SELECT * 
FROM all_null AS a, test AS b 
WHERE (a.x BETWEEN b.x AND b.x);
