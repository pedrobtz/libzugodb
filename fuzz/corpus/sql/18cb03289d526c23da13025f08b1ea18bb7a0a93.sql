SELECT l.id FROM l LEFT JOIN r ON l.id = r.id GROUP BY l.id ORDER BY count(r.*), l.id;
