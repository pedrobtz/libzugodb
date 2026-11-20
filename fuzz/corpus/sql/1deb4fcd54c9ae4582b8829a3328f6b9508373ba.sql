SELECT len(lttb(ts::TIMESTAMP_S, v, 3 ORDER BY ts)),
       len(lttb(ts::TIMESTAMP_MS, v, 3 ORDER BY ts)),
       len(lttb(ts::TIMESTAMP_NS, v, 3 ORDER BY ts))
FROM (SELECT TIMESTAMP '2020-01-01' + INTERVAL (i) HOUR AS ts, i::DOUBLE AS v FROM range(20) t(i))
