CREATE OR REPLACE TABLE prf_probe_string_long_same_prefix AS
SELECT * FROM (VALUES
	('aaaa_shared_probe_suffix_0001', 0, 1),
	('aaaa_shared_probe_suffix_0002', 1, 2),
	('zzzz_probe_suffix_9999', 0, 3),
	(NULL, 0, 4)
) AS t(k, g, id);
