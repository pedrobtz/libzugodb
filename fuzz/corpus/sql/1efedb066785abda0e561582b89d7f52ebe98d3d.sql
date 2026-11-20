SELECT d, DATE_PART(['isoyear', 'week', 'yearweek'], d) AS parts
FROM dates
ORDER BY 1;
