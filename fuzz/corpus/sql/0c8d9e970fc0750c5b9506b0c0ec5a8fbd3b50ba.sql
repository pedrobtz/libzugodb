SELECT row(NULL::INT, 8) IN (SELECT * FROM (VALUES (2, 6), (3, 7)) rhs(x, y)),
       row(NULL::INT, 8) NOT IN (SELECT * FROM (VALUES (2, 6), (3, 7)) rhs(x, y))
