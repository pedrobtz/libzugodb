SELECT finalize(combine(to_aggregate_state({'count': 1, 'value': 0}, 'avg', ['{type}']),
                        to_aggregate_state({'count': 1, 'value': 86400000001}, 'avg', ['{type}'])));
