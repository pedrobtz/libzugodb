SELECT __mark_join_marker FROM (SELECT NULL::INTEGER AS dummy) l JOIN BY(TYPE MARK) (SELECT 1 AS i) r ON 1=r.i;
