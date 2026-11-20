SELECT a FROM residual_vectors_probe JOIN BY(TYPE SEMI) residual_vectors_build ON a<2 AND CASE WHEN a+b=2 THEN NULL ELSE a+b=3 END;
