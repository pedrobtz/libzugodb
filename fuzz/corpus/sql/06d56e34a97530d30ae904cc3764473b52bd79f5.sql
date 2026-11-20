SELECT printf('%.17g', fsum(x)),
	printf('%.17g', kahan_sum(x)),
	printf('%.17g', sumkahan(x)),
	printf('%.17g', sum(x)),
	printf('%.17g', favg(x)),
	printf('%.17g', avg(x))
FROM doubles;
