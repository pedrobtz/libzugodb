SELECT i FROM integers WHERE i = ALL(SELECT i FROM integers WHERE i=1)
