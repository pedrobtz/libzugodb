CREATE TABLE combined_big_string_state AS
WITH state1 AS (
    SELECT g, first(big_str) EXPORT_STATE as state1
    FROM big_string_test 
    WHERE r < 10
    GROUP BY g
),
state2 AS (
    SELECT g, first(big_str) EXPORT_STATE as state2
    FROM big_string_test 
    WHERE r >= 10
    GROUP BY g
)
SELECT 
    COALESCE(s1.g, s2.g) as g,
    COMBINE(s1.state1, s2.state2) as combined_state
FROM state1 s1
FULL OUTER JOIN state2 s2 ON s1.g = s2.g;
