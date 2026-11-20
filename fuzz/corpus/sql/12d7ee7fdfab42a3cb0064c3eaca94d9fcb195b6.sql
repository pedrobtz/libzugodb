UPDATE dim SET d_label = CASE WHEN d_key = 'KEY_0' THEN NULL ELSE repeat(d_key, 32) END
