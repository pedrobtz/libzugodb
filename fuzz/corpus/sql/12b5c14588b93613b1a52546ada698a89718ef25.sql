select a, sum(b) from (select [42, 43, 44] union all select [45, NULL, 46]) t(a), (select unnest(t.a)) t2(b) group by a order by a;
