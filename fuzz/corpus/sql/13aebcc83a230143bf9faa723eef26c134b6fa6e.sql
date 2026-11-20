select count(*) from times inner join timestamp on (timestamp.i::TIME = times.i)
