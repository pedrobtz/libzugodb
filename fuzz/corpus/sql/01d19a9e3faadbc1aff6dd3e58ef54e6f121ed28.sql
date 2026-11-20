SELECT i, i >= ANY(SELECT i FROM integers WHERE i>10) FROM integers ORDER BY i;
