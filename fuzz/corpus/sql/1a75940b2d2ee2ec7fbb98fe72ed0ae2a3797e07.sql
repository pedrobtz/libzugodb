SELECT lhs.ts, rhs.ts,
	lhs.ts < rhs.ts, lhs.ts <= rhs.ts,
	lhs.ts = rhs.ts, lhs.ts <> rhs.ts,
	lhs.ts >= rhs.ts, lhs.ts > rhs.ts,
FROM specials lhs, specials rhs
ORDER BY 1, 2
