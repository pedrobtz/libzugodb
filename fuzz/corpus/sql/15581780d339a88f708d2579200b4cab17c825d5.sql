CREATE TABLE probe0 AS 
	SELECT range::VARCHAR AS begin
	FROM range(0,10)
;
