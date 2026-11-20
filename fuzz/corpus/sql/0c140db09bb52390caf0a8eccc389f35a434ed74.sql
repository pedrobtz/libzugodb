SELECT row(union_value(num := NULL::INT)::UNION(num INT, str VARCHAR), 0) IN (
	SELECT * FROM (VALUES (union_value(num := 1)::UNION(num INT, str VARCHAR), 0)) rhs(a, b)
)
