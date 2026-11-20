select array(select * from unnest(['a', 'b']) as _t(u) order by if(u='a',100, 1)) as out;
