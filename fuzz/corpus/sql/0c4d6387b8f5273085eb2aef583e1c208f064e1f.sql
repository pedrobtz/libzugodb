SELECT MEDIAN(range::timestamptz) 
FROM range('2024-01-01'::date, '2024-01-11'::date, INTERVAL 1 DAY);
