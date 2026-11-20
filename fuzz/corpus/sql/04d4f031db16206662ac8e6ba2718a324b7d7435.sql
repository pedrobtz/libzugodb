select count(*), course AS crs, type AS tp
        from students
        group by grouping sets((crs, tp), (tp))
        order by 1, 2, 3;
