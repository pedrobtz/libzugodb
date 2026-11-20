WITH files AS (
	SELECT f.*, payload FROM filenames f, range(3) t(payload)
), extracted AS (
	SELECT regexp_extract(filename, 'rundate_(\d+-\d+-\d+)_pass_(\d+)', ['rundate', 'pass']) AS groups, payload 
	FROM files
)
SELECT groups.rundate::DATE AS rundate, groups.pass::SMALLINT AS PASS, SUM(payload)
FROM extracted
WHERE LENGTH(groups.rundate) > 0
GROUP BY ALL
