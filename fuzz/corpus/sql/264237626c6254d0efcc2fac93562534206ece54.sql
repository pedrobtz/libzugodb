CREATE TABLE dim AS SELECT 'KEY_' || i AS d_key, 'LABEL_' || (i % 5) AS d_label FROM range(100) t(i)
