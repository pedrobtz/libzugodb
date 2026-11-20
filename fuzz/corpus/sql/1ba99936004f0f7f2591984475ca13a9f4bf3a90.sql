EXPLAIN SELECT * FROM single_scan
WHERE i0 >= 5 AND i1 >= i0 AND i2 >= i1 AND i3 >= i2 AND i4 >= i3
	AND i5 >= i4 AND i6 >= i5 AND i7 >= i6;
