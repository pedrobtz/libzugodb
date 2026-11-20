select g, finalize(combine(sum(d) EXPORT_STATE, sum_state)) combined_sum from dummy join state using (g) group by g, sum_state ORDER BY g;
