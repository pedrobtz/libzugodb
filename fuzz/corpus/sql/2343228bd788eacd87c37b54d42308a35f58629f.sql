SELECT * FROM lineitem WHERE (SELECT SUM(l_orderkey) > 0)
