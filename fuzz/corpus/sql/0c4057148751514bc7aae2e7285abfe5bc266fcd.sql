FROM wide_ranges l
SEMI JOIN narrow_ranges r
	   ON l.start < r.stop
	  AND r.start < l.stop
      AND l.price + r.bid < 400
ORDER BY id;
