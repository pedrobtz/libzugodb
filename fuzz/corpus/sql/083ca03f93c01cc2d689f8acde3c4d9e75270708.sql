SELECT printf('%.17g', finalize(combine(
		(SELECT fsum(x) EXPORT_STATE FROM doubles),
		(SELECT fsum(x) EXPORT_STATE FROM doubles WHERE false)))),
	printf('%.17g', finalize(combine(
		(SELECT favg(x) EXPORT_STATE FROM doubles),
		(SELECT favg(x) EXPORT_STATE FROM doubles WHERE false))));
