SELECT MODE(order_occurrences ORDER BY order_occurrences DESC) FROM (
VALUES
	(500, 1),
	(1000, 2),
	(800, 3),
	(1000, 4),
	(500, 5),
	(550, 6),
	(400, 7),
	(200, 8),
	(10, 9)
)items_per_order(order_occurrences, item_count);
