select o_orderkey, o_clerk, o_orderstatus, o_totalprice from orders_small
                    order by o_orderkey NULLS FIRST,
                    o_clerk NULLS FIRST, o_orderstatus NULLS FIRST,
                    o_totalprice DESC NULLS LAST limit 360
