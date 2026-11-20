SELECT __mark_join_marker FROM (VALUES (row(1,2))) p(c) JOIN BY(TYPE MARK) (VALUES (row(1,NULL::INT))) b(d) ON c < d;
