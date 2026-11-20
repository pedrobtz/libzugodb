CREATE TABLE grouped AS SELECT range % 3 g, range x, range + 1 y FROM range(30);
