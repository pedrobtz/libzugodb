select fill(i order by 10-i, i * i) over (order by i) from range(3) tbl(i);
