SELECT count(*) FROM (
    SELECT g, list_sort(approx_top_k(v, 5 ORDER BY v)) AS ordered,
              list_sort(approx_top_k(v, 5)) AS unordered
    FROM ordered_groups GROUP BY g
) WHERE ordered IS DISTINCT FROM unordered;
