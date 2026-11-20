SELECT str, 
  REGEXP_EXTRACT_ALL(str,'a[bc]*d') AS matched
FROM (
  VALUES ('ad'), ('abd'), ('acd'), ('abcd'), ('abcbbcd'), ('abce')
) AS t(str)
