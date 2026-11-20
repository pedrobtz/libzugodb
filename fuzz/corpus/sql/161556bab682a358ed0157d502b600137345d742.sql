SELECT COUNT(*) FROM (SELECT 'x
x') t1(a) JOIN (SELECT regexp_replace('hello
world', '.*', 'x', 'ng')) t2(a) USING (a)
