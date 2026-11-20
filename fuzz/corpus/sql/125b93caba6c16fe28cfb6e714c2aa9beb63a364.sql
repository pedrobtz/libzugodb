SELECT sum(i) OVER (ORDER BY i GROUPS 1 PRECEDING) 
FROM generate_series(1,10) AS _(i)
ORDER BY i
