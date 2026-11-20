select * from t1 semi join t2 on t1.a < t2.b or t1.b < t2.b order by all;
