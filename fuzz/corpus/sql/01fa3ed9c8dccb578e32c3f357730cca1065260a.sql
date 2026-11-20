create table calendar as SELECT *
FROM range(DATE '2022-01-01', DATE '2024-02-01', INTERVAL '1' MONTH);
