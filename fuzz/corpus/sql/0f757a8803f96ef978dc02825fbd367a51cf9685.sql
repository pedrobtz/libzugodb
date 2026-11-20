SELECT (NULL::INT, 2) <> ANY (
	SELECT * FROM (VALUES (1, 2)) rhs(a, b)
)
