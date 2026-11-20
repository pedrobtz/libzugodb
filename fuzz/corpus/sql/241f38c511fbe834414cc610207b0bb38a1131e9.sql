create table dates as select date '1992-01-01' + concat(i, ' months')::interval as d from range(100) tbl(i);
