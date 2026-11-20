select count(*), course, type
        from students
        group by grouping sets((course, type), (type))
        order by 1, 2, 3;
