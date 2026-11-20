select
  array(select distinct i from t order by t.i desc) as a
