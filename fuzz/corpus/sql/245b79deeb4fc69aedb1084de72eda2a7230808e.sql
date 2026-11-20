WITH cte as (
	select {a: 21, b: [1,2,3]}::VARIANT a
)
select IF(a > [1,2,3], 1, 0) from cte
