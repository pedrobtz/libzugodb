SELECT (SELECT SUM(i)+SUM(i1.i) FROM integers)>ANY(SELECT i FROM integers WHERE i>MIN(i1.i)) FROM integers i1;
