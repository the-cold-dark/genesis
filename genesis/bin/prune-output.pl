while (<>) {
	($a, $b) = split(/> /, $_, 2);
	print $b;
}

