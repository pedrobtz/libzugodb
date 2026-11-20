SELECT 
    g,
    finalize(first_state) as finalized_from_stored
FROM stored_big_string_state
ORDER BY g;
