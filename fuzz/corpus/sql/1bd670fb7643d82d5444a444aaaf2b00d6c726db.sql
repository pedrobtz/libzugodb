SELECT i, fill(CASE WHEN i = 1 THEN NULL ELSE i END::{type}) OVER (ORDER BY i::{type}) AS filled
FROM range(3) r(i)
QUALIFY i IS DISTINCT FROM filled;
