explain select nth_value(i, 2) over (ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) 
from range(5) tbl(i)
