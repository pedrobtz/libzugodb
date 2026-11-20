SELECT col1 IN (SELECT ColID + another_T.col1 FROM tbl_ProductSales) FROM another_T GROUP BY col1;
