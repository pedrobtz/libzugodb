create table lists as select array[i] l from generate_series(0,5,1) tbl(i);
