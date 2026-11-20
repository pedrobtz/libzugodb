INSERT INTO big SELECT (i % 3) = 0, ['a', '', 'dup', 'zzz'][(i % 4) + 1] FROM generate_series(1, 30) t(i);
