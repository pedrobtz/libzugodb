SELECT p.ts, e.value
FROM range(0,10) p(ts) ASOF JOIN events0 e
ON 1 = 1 AND p.ts >= e.begin
ORDER BY p.ts ASC
