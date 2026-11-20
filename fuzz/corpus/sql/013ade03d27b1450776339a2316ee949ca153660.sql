CREATE VIEW cse AS 
WITH t AS (SELECT
    string_agg(activity_name, ',' order by ts asc, activity_name) as trace,
    1 as cnt
from
    eventlog
group by case_id
)
SELECT
    trace,
    sum(cnt) as cnt_trace,
    sum(cnt_trace) over () as cnt_total,
    sum(cnt) / sum(cnt_trace) over () as rel,
    sum(cnt_trace) over (
         order by cnt_trace desc 
         ROWS between UNBOUNDED PRECEDING and CURRENT ROW) 
      / sum(cnt_trace) over () 
      as rel
from t
group by trace
order by cnt_trace desc
