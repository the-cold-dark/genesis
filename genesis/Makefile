all:
	@if test ! -f src/Makefile; then ./configure; fi
	cd src/; make
	mv -f src/genesis genesis
	mv -f src/coldcc coldcc
