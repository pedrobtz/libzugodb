select bool_and(d > '2021-02-09') over (partition by d)
    from t order by d;
