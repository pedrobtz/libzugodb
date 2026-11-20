CREATE TABLE src AS
SELECT (i // 2)::BIGINT AS g, ((i * 7) % 96)::INTEGER AS v
FROM range(2000000) t(i);
