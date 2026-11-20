WITH actual AS (
    SELECT p.i, p.k FROM probe p JOIN build b ON p.k=b.k WHERE p.g>=3
), expected AS (
    SELECT i, k FROM probe, range(2)
    WHERE g>=3 AND k BETWEEN -40 AND {width}-41 AND k<>-39
)
SELECT count(*) FROM (
    (TABLE actual EXCEPT ALL TABLE expected)
    UNION ALL
    (TABLE expected EXCEPT ALL TABLE actual)
);
