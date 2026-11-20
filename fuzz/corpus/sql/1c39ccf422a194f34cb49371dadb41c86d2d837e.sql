select
	range,
	count(*) as n
from scd2
inner join calendar
		on range <= ifnull(range_end,'2099-01-01') and range_start <= range
group by range
order by range
