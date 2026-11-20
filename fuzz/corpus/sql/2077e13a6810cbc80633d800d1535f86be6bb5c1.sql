SELECT count(*) FROM l ASOF JOIN r ON ('\x00' || l.g::VARCHAR)::BLOB = ('\x00' || r.g::VARCHAR)::BLOB AND l.ts >= r.ts;
