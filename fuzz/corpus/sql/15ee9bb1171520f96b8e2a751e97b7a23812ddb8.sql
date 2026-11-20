SELECT l_returnflag, max(l_orderkey, 3) FROM lineitem GROUP BY ALL ORDER BY ALL;
