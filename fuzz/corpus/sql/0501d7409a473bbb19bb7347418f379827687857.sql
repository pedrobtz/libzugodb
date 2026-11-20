select (select MIN(val) from unnest(a) t(val)) from (select ARRAY[1, 2, 3, NULL]) t(a);
