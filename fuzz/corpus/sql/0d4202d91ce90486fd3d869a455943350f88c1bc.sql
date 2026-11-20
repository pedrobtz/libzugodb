SELECT col1 IN (SELECT ColID + col1 FROM tbl_ProductSales) FROM another_T GROUP BY col1;
