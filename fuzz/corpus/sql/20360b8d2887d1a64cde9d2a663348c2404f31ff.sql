SELECT count(*) FROM (
    SELECT n, skewness(x) AS sk
    FROM (SELECT {val} AS x, n FROM range(1, 1001) r(n), range(0, 1001) k(k) WHERE k < n)
    GROUP BY n
) WHERE sk IS NOT NULL
