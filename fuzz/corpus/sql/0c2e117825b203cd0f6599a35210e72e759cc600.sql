CREATE TABLE r AS SELECT range::INT i, {'y': range, 'z': [range, range]::INT[2]} s, union_value(k := range::INT) u FROM range(1000);
