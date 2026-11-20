SELECT st.n, st.mean, st.m2, st.m3, st.m4
FROM (SELECT (kurtosis(d) EXPORT_STATE)::STRUCT(n UBIGINT, mean DOUBLE, m2 DOUBLE, m3 DOUBLE, m4 DOUBLE) st FROM dummy);
