select UNNEST(string_split_regex('abc', '(|abc)'))
