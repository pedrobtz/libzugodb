SELECT count(r.*) FILTER (WHERE l.id > 2) FROM l LEFT JOIN r ON l.id = r.id;
