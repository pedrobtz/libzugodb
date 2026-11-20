(
  SELECT q.user_id, t.product_id FROM users q
    INNER JOIN products t NEAREST 2 BY DISTANCE array_distance(q.embedding, t.embedding)
  EXCEPT
  SELECT q.user_id, t.product_id FROM users q
    INNER JOIN (SELECT * FROM products ORDER BY array_distance(q.embedding, products.embedding) LIMIT 2) t ON TRUE
)
UNION ALL
(
  SELECT q.user_id, t.product_id FROM users q
    INNER JOIN (SELECT * FROM products ORDER BY array_distance(q.embedding, products.embedding) LIMIT 2) t ON TRUE
  EXCEPT
  SELECT q.user_id, t.product_id FROM users q
    INNER JOIN products t NEAREST 2 BY DISTANCE array_distance(q.embedding, t.embedding)
);
