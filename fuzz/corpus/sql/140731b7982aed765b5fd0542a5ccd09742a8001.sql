EXPLAIN SELECT * FROM orders o
INNER JOIN lineitem l ON o.o_orderkey = l.l_orderkey
    AND o.o_totalprice + l.l_extendedprice > 250;
