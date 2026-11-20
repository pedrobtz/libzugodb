SELECT COUNT(*) FROM p WHERE p.pb NOT IN (SELECT x.f FROM v x JOIN v y ON x.s = y.s);
