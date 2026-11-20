CREATE VIEW state_view AS SELECT g, skewness(d) EXPORT_STATE skewness_state FROM dummy GROUP BY g ORDER BY g;
