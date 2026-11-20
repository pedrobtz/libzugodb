SELECT q.user_id, t.product_id FROM users q
  INNER JOIN products t NEAREST BY DISTANCE array_distance(q.embedding, t.embedding)
  ORDER BY q.user_id;
