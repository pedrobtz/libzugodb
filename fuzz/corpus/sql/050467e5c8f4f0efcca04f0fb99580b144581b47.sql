explain
select t1.x, t1.y
from (
    select (case when x < 100 then null else 99 end) x, (case when x < 100 then 99 else 99 end) y
    from tt2
) t1 left join tt2 t2
on t1.x < t2.x and t1.y < t2.x
order by t1.x nulls first, t1.y nulls first;
