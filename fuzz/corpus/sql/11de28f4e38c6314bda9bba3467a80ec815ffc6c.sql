SELECT * FROM left_table l
ASOF JOIN right_table r
ON l.symbol = r.symbol AND l.ts >= r.ts AND l.price + r.bid > 300;
