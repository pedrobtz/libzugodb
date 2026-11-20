WITH files AS (
	SELECT f.*, payload FROM filenames f, range(3) t(payload)
), extracted AS (
	SELECT regexp_extract(filename, 'rundate_(\d+-\d+-\d+)_([a-z]+?)_(\d+)', ['rundate', 'opt', 'pass']) AS groups, payload 
	FROM files
)
SELECT groups.rundate::DATE AS rundate, groups.opt AS opt, groups.pass::SMALLINT AS pass, SUM(payload)
FROM extracted
WHERE LENGTH(groups.rundate) > 0
GROUP BY ALL
