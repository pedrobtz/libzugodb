SELECT name, city, row_number() OVER (PARTITION BY city) AS row_id
FROM db_city;
