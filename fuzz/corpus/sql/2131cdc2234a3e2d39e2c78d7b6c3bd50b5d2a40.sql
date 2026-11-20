CREATE TABLE events as FROM (VALUES
	(TIMESTAMP '1992-09-20 20:38:40', 'Event A'),
	(TIMESTAMP '1992-09-20 21:45:15', 'Event B'),
	(TIMESTAMP '1992-09-20 22:15:30', 'Event C')) t(event_time, event_name);
