SELECT finalize(to_aggregate_state({'precision': 10, 'registers': [0, 0, 0]::UTINYINT[]},
                                  'approx_count_distinct', ['ANY']));
