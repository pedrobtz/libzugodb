SELECT * FROM unit2 JOIN (VALUES (2000)) t(total_profit) ON (t.total_profit = unit2.total_profit AND t.total_profit=unit2.also_total_profit);
