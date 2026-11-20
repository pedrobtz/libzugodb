SELECT regexp_extract_all('ab ac a', '(a)(b)?(c)?', ['a','b','c']) AS res;
