CREATE TABLE first_state AS SELECT g, first(d) EXPORT_STATE f, last(d) EXPORT_STATE l FROM dummy GROUP BY g;
