SELECT i=ANY(SELECT i FROM integers WHERE i=i1.i) FROM integers i1 ORDER BY i;
