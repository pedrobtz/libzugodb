PREPARE p AS SELECT id FROM t WHERE s ILIKE '%' || ? || '%' ORDER BY id
