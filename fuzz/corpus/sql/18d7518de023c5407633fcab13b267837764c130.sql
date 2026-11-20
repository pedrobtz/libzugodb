explain
select l.k100 as lid, r.k100 as rid
from tleft l 
full join tleft r 
on l.b100 < r.e100
and l.k100 + r.k100 < 10
order by all
