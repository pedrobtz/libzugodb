EXPLAIN
SELECT east.rid, west.rid
FROM east, west
WHERE east.dur < west.time AND east.rev > west.cost
ORDER BY 1, 2
