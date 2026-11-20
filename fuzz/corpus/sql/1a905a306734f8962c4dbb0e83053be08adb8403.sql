select variant_comparator(variant_group_object(k, v)) = variant_comparator({
    'null_value': NULL,
    'number': 2
}::VARIANT)
from (values
    ('null_value', NULL::VARIANT),
    ('number', 2::VARIANT)
) t(k, v);
