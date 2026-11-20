WITH q AS MATERIALIZED (
    SELECT id, v FROM srs_zero_probe ORDER BY v, id
)
SELECT id, v, signbit(v), 1.0 / v FROM q ORDER BY id;
