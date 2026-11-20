SELECT    k, bitstring_agg(idx)
FROM      t1_v
JOIN      el_ids USING (el)
GROUP BY  k;
