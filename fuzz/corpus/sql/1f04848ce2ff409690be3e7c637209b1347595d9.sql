CREATE TABLE t AS SELECT range i, range % 3 g, (range / 4)::DECIMAL(9,2) d FROM range(6);
