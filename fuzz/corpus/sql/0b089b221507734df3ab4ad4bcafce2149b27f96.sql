SELECT x.* FROM generated_outer o, LATERAL (SELECT o.*) x ORDER BY ALL;
