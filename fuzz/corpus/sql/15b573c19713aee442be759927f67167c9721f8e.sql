SELECT i, (SELECT row_number() OVER (ORDER BY i)) FROM integers i1 ORDER BY i;
