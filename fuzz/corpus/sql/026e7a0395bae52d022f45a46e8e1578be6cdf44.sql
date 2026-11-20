EXPLAIN
SELECT t1.x, t2.x
FROM 'test/sql/join/iejoin/overlap.left.csv' t1, 'test/sql/join/iejoin/overlap.right.csv' t2
WHERE t1.y > t2.y AND t1.x < t2.x;
