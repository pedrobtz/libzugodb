SELECT regexp_extract_all('AbCdEfCg', '([[:lower:]]+)C([[:lower:]]+)', ['g1','g2']) AS res;
