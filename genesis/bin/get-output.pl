while (<>) {
	($a, $b) = split(/^\tOutput: /);
	while ($b) {
		print $b;
		($a, $b) = split(/^\t\t/, <>);
	}
}

