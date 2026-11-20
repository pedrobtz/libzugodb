SELECT count(*) FROM a_t ASOF JOIN b_t ON a_t.k = b_t.k AND a_t.t >= b_t.t AND a_t.m NOT IN ('sad', 'ok', 'happy');
