CREATE TABLE dictionary_inputs AS
SELECT i, (i//64)%8 AS g, CASE WHEN i%7=0 THEN i%101-50 END::INTEGER AS v
FROM range(16513) t(i);
