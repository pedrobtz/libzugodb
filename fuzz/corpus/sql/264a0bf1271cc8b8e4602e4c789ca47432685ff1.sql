SELECT lhs.dt, rhs.dt,
	lhs.dt < rhs.dt, lhs.dt <= rhs.dt,
	lhs.dt = rhs.dt, lhs.dt <> rhs.dt,
	lhs.dt >= rhs.dt, lhs.dt > rhs.dt,
FROM specials lhs, specials rhs
ORDER BY 1, 2
