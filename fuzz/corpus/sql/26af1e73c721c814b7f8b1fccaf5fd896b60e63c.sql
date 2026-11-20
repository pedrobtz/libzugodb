CREATE TABLE big_dim AS SELECT range::INTEGER AS id, 'val_' || range AS label FROM range(1, 10001)
