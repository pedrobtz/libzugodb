SELECT '%' ILIKE (chr(0) || '%') ESCAPE chr(0),
       '%' NOT ILIKE (chr(0) || '%') ESCAPE chr(0),
       '%' ILIKE (chr(0) || '%') ESCAPE '',
       (chr(0) || '%') ILIKE (chr(0) || '%') ESCAPE ''
