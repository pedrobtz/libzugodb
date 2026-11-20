SELECT * FROM integers WHERE EXISTS (SELECT i, i + 2 FROM integers)
