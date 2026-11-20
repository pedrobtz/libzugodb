SELECT p.key, p.ts, e.value
FROM 
	probes p
LEFT JOIN (
	SELECT key, value, begin, 
		LEAD(begin, 1, 'infinity'::DOUBLE) OVER (PARTITION BY key ORDER BY begin ASC) AS end
	FROM events
) e
ON p.key = e.key AND p.ts >= e.begin AND p.ts < e.end
ORDER BY 1, 2 ASC NULLS FIRST
