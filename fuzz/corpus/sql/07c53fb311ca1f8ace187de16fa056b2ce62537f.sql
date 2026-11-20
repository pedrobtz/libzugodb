WITH subquery AS (SELECT i, lag(i) OVER named_window FROM ( VALUES (1), (2), (3)) AS t (i)) SELECT * FROM subquery window named_window AS ( ORDER BY i);
