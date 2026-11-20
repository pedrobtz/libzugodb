SELECT * from qt QUALIFY row_number() over (PARTITION BY b ORDER BY c) = 1 ORDER BY b
