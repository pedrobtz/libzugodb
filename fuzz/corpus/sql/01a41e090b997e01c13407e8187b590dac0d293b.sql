SELECT a ILIKE ('goo' || chr(0) || 'se'), a NOT ILIKE ('goo' || chr(0) || 'se') FROM unicode_possible ORDER BY a
