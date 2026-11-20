SELECT __mark_join_marker FROM (VALUES (1,2)) p(a,c) JOIN BY(TYPE MARK) (VALUES (1,3)) b(b,d) ON a=b AND c>d;
