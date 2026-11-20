SELECT str, 
  REGEXP_EXTRACT_ALL(str,'X[\d]?Y[\d]?') AS matched
FROM (
  VALUES ('XY'),('X1Y'),('X123Y'),('X1Y23')
) AS t(str)
