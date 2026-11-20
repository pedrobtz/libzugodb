SELECT row(array_value(1, 2), 0) IN (
	       SELECT * FROM (VALUES (array_value(1, NULL::INT), 0)) rhs(a, b)
       ),
       row(map([1, 2], [1, 2]), 0) IN (
	       SELECT * FROM (VALUES (map([1, 2], [1, NULL::INT]), 0)) rhs(a, b)
       ),
       row({'a': 1, 'b': 2}, 0) IN (
	       SELECT * FROM (VALUES ({'a': 1, 'b': NULL::INT}, 0)) rhs(a, b)
       ),
       row(union_value(num := 1)::UNION(num INT, str VARCHAR), 0) IN (
	       SELECT *
	       FROM (VALUES (union_value(num := NULL::INT)::UNION(num INT, str VARCHAR), 0)) rhs(a, b)
       )
