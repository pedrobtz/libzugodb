SELECT __mark_join_marker
FROM range(2) p(i) JOIN BY (TYPE MARK)
(SELECT i FROM range(12) b(i) WHERE random()<0) b
ON nextval('scalar_operand')=b.i;
