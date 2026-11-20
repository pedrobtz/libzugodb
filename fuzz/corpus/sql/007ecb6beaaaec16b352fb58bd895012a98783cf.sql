SELECT DaysToManufacture, StandardCost, (SELECT cost FROM
	(SELECT DaysToManufacture, StandardCost) AS SourceTable
	PIVOT
	(
	  AVG(StandardCost)
	  FOR DaysToManufacture IN (0, 1, 2, 3, 4)
	) AS PivotTable
	UNPIVOT
	(
		cost FOR days IN (0, 1, 2, 3, 4)
	)
)
FROM Product
