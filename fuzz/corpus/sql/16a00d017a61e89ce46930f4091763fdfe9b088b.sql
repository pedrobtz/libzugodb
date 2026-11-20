SELECT COUNT(*) FROM bigtbl JOIN smalltbl ON (bigtbl.i BETWEEN low AND high AND bigtbl.i IS NOT DISTINCT FROM high - low)
