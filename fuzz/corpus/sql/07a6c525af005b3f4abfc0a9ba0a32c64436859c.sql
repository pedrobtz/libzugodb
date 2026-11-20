SELECT id, parse_path(path[1], separator)
FROM (VALUES (0, ['a/b\c'], NULL), (1, ['a/b\c'], 'forward_slash'), (2, ['a/b\c'], 'backslash')) t(id, path, separator)
WHERE id <> 0 ORDER BY id;
