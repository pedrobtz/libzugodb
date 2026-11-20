SELECT * FROM left_table SEMI JOIN (SELECT a as foo from right_table where b = 1) buzz ON left_table.a = buzz.foo
