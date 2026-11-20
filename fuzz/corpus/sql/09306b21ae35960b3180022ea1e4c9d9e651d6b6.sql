SELECT count(*), count(rid), currval('pair_volatile_not_in_no_pullup_seq')
FROM (VALUES (1, 1), (2, 1)) l(lid, a)
LEFT JOIN (VALUES (10, 1), (20, 1)) r(rid, b) ON l.a NOT IN (
	SELECT r.b
	WHERE nextval('pair_volatile_not_in_no_pullup_seq') = 2
);
