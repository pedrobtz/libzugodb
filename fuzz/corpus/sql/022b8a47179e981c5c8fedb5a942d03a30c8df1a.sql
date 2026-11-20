SELECT count(*) FROM (SELECT *, count(*) OVER () AS n FROM owner_walk) q
WHERE person = 1 AND n = 15;
