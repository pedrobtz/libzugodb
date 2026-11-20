explain SELECT * 
FROM big_table c
LEFT OUTER JOIN single_col_table hd ON hd.col1=c.col1 
AND (
    FALSE
);
