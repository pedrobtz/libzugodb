SELECT any_value(CASE WHEN i % 5 = 0 THEN {'value': i, 'items': [i, i + 1]} END) IS NOT NULL
FROM range(3000) r(i)
GROUP BY i >= 2500
ORDER BY i >= 2500;
