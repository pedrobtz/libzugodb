SELECT p.begin, e.begin, e.value
FROM probe0 p ASOF RIGHT JOIN events0 e
ON p.begin <= e.begin
ORDER BY ALL ASC
