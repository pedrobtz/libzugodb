WITH cte AS (
    SELECT c
    FROM tbl
    OFFSET 500 LIMIT 500
)
SELECT sum(cast(regexp_escape(c) = '1\?ch\@racter\$' as int))
FROM cte
