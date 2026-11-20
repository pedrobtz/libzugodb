select  regr_syy(v, v2) over (partition by k)
    from aggr;
