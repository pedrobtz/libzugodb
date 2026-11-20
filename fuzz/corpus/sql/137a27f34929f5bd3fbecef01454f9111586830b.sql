select md5(s), md5('1') from strings where s::INTEGER BETWEEN 1 AND 3 ORDER BY s
