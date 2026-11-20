SELECT count(*) FILTER (l = -1) = {rows} FROM (SELECT {fun}(i, ({offset})::BIGINT, -1) OVER (PARTITION BY i % 3) AS l FROM range({rows}) t(i))
