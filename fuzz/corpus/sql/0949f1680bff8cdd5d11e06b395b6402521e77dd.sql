select course, count(*) from students group by rollup () order by 1, 2;
