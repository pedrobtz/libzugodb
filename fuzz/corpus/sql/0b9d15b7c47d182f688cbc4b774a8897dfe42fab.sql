SELECT CASE
	  WHEN ( approx_quantile between (true_quantile - 100) and (true_quantile + 100) )
		  THEN TRUE
		  ELSE FALSE
	  END
	  FROM (SELECT approx_quantile(r, 0.5) as approx_quantile ,quantile(r,0.5) as true_quantile FROM quantile) AS T
