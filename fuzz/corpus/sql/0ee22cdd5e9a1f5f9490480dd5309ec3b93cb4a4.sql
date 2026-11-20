WITH states AS (
	SELECT
		covar_pop(x, y) EXPORT_STATE AS pop_state,
		covar_samp(x, y) EXPORT_STATE AS samp_state
	FROM (VALUES (0, 1, 2), (0, 2, 4), (1, 3, 6), (1, 4, 8)) t(g, x, y)
	GROUP BY g
)
SELECT finalize(combine_aggr(pop_state)), finalize(combine_aggr(samp_state)) FROM states;
