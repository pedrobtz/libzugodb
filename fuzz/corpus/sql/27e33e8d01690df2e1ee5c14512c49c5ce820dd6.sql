select count(*), course, type
        from students
        group by grouping sets((2, 3), (3))
        order by 1, 2, 3;
