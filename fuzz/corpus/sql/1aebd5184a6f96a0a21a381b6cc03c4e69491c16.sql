select course as crs, type as tp, count(*) from students group by grouping sets (rollup (crs)), (), tp order by 1, 2, 3;
