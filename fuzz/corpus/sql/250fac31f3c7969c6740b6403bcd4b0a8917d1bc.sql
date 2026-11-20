select * from (select 42) t(a) join lateral (select t.a + 1) t2(b) on (a+b>=80);
