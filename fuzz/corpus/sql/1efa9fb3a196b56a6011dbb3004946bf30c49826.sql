select * from (select ARRAY[42, 43, 44]) t(a) join lateral (select * from unnest(t.a)) t2(b) on (true) ORDER BY b;
