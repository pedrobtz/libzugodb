SELECT p.ts, e.value
FROM 
	range(0,10) p(ts)
LEFT JOIN (
	SELECT value, begin, 
		LEAD(begin, 1, 'infinity'::DOUBLE) OVER (ORDER BY begin ASC) AS end
	FROM events0
) e
ON p.ts >= e.begin AND p.ts < e.end
ORDER BY p.ts ASC NULLS FIRST
