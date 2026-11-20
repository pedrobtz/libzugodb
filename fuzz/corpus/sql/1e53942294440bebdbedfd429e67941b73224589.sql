WITH cte as (
	select '12'::VARIANT a
)
select IF(a == 12, 1, 0) from cte
