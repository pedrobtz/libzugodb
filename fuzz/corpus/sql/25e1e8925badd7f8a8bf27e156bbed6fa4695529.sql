SELECT p.begin, e.value
FROM range(0,10) p(begin) ASOF LEFT JOIN events0 e
USING (begin)
ORDER BY p.begin ASC NULLS FIRST
