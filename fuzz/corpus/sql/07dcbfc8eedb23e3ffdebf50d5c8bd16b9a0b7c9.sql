SELECT lttb(ts, v, 3 ORDER BY ts)
FROM (SELECT TIMESTAMP '2020-01-01' + INTERVAL (i) DAY AS ts, (i * i)::DOUBLE AS v FROM range(5) t(i))
