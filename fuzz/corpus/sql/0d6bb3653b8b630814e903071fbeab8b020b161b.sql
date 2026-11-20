SELECT p.i,__mark_join_marker FROM probe p JOIN BY (TYPE MARK) build b ON p.i IS NOT DISTINCT FROM NULL;
