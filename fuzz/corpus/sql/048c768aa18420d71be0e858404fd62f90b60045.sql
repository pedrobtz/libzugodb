select x, a
from
(values (1), (2)) t2(x),
lateral (select sum(a) from (select 1, 2 where 1 = x) t(a, b) group by grouping sets ((), (b), (a, b))) t3(a)
order by all;
