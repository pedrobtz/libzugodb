SELECT quantile_cont(d::{type}, [0.25, 0.5, 0.75])
FROM range(0,100) tbl(d)
