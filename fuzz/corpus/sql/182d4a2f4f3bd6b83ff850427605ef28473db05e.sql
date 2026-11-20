SELECT * 
FROM empty_unsorted 
WHERE(NOT(false = ANY([]))) 
ORDER BY(~((SUM(true) OVER() - SUM(true) OVER())::INT)) ASC;
