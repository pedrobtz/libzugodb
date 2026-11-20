SELECT o.o_orderkey, o.o_custkey, o.o_totalprice, l.l_orderkey, l.l_partkey, l.l_quantity, l.l_extendedprice
FROM orders o
INNER JOIN lineitem l ON o.o_orderkey = l.l_orderkey
    AND o.o_totalprice < l.l_extendedprice
    AND o.o_totalprice + l.l_extendedprice > 250;
