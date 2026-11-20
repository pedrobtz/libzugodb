SELECT count(*) FROM (SELECT 'CHF' AS ccy, id FROM fact) t WHERE ccy IN (SELECT k FROM dim)
