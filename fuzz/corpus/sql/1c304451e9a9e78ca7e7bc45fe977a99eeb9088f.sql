SELECT o.o_orderkey, o.o_custkey, o.o_totalprice, o.o_orderstatus
FROM orders o
WHERE o.o_orderkey NOT IN (
    SELECT l.l_orderkey
    FROM lineitem l
    WHERE l.l_orderkey = o.o_orderkey
    AND o.o_totalprice + l.l_extendedprice > 250
);
