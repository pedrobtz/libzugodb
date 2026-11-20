SELECT * FROM left_table l
ASOF LEFT JOIN right_table r
ON l.symbol = r.symbol AND l.ts >= r.ts AND l.price + r.bid > 300;
