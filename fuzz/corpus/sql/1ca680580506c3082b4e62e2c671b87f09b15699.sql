select g, finalize(sum_state) * 2 combined_sum from (select g, sum(d) EXPORT_STATE sum_state from dummy where g >= 5 GROUP BY g union all SELECT * FROM state2) ORDER BY g;
