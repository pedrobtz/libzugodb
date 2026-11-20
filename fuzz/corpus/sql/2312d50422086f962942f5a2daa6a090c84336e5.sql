CREATE OR REPLACE TABLE scatter_result AS SELECT g,
       sum(v*1) FILTER (WHERE i>={dense_start} OR day=0) AS s0,
       sum(v*2) FILTER (WHERE i>={dense_start} OR day=1) AS s1,
       sum(v*3) FILTER (WHERE i>={dense_start} OR day=2) AS s2,
       sum(v*4) FILTER (WHERE i>={dense_start} OR day=3) AS s3,
       sum(v*5) FILTER (WHERE i>={dense_start} OR day=4) AS s4,
       sum(v*6) FILTER (WHERE i>={dense_start} OR day=5) AS s5,
       sum(v*7) FILTER (WHERE i>={dense_start} OR day=6) AS s6
FROM clustered_inputs GROUP BY g;
