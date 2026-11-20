explain
select
	count(*)
from snapshot_data data
join cal_last_13 cal
	on data.snapshot_ts >= cal.start_ts
	and data.snapshot_ts <= cal.end_ts
join cal_last_13 cal2
	on data.snapshot_ts >= cal2.start_ts
	and data.snapshot_ts <= cal2.end_ts
