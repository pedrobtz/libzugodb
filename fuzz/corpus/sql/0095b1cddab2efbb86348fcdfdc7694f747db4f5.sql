select variant_comparator(variant_group_object(k, v)) = variant_comparator({
    'date': DATE '2020-01-02',
    'timestamp': TIMESTAMP '2020-01-02 03:04:05',
    'blob': 'abc'::BLOB,
    'map': MAP {'x': 1, 'y': 2}::VARIANT
}::VARIANT)
from (values
    ('date', DATE '2020-01-02'::VARIANT),
    ('timestamp', TIMESTAMP '2020-01-02 03:04:05'::VARIANT),
    ('blob', 'abc'::BLOB::VARIANT),
    ('map', MAP {'x': 1, 'y': 2}::VARIANT)
) t(k, v);
