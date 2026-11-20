SELECT CASE
	  WHEN (approx_quantile between (true_quantile - (sumr * 0.01)) and (true_quantile + (sumr * 0.01)))
		  THEN TRUE
		  ELSE FALSE
	  END
	  FROM (SELECT approx_quantile(r, 0.1) as approx_quantile, quantile(r, 0.1) as true_quantile, SUM(r) as sumr from quantile) AS T
