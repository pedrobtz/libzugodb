select l.* from test_int l anti join test_int_del r on l.k = r.pk;
