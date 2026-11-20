SELECT count(*) FROM l ASOF JOIN r ON row(l.g, 1) = row(r.g, 1) AND l.ts >= r.ts;
