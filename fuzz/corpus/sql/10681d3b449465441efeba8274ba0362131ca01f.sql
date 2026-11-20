SELECT o.o_orderkey, o.o_custkey, o.o_totalprice, o.o_orderstatus
FROM orders o
ANTI JOIN lineitem l ON o.o_orderkey = l.l_orderkey
    AND o.o_orderstatus != 'O';
