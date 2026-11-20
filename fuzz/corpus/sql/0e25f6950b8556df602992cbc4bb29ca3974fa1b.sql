SELECT (union_value(num := 1)::UNION(num INT, str VARCHAR), 2) <> ANY (
	SELECT * FROM (VALUES (union_value(num := 1)::UNION(num INT, str VARCHAR), 3)) rhs(u, v)
)
