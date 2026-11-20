SELECT count(l.*), count(r.*) FROM l FULL OUTER JOIN r ON l.id = r.id;
