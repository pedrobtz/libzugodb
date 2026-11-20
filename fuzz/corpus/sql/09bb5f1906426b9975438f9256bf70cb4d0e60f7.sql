SELECT grp, arg_max_nulls_last(arg, val) FROM tbl GROUP BY grp ORDER BY grp
