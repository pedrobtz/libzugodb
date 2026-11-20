SELECT * FROM integers WHERE i=(SELECT MAX(i) FROM integers)
