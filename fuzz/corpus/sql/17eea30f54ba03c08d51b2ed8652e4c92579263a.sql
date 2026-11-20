CREATE TABLE fact1 AS SELECT 'KEY_' || (i % 100) AS f1_dkey, 'CUST_' || i AS f1_ckey FROM range(262200) t(i)
