with joined as (
	select lhs.k l, rhs.k r
	from states lhs 
	inner join states rhs
	   on lhs.b < rhs.e
	  and rhs.b < lhs.e
	  and lhs.k = rhs.k
)
select count(*) 
from joined
