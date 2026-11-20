SELECT count(*) FROM (SELECT 'GBP' AS ccy, id FROM fact) t WHERE (ccy IN (SELECT k FROM dim))
