all: config
	@cd src; make $@

clean: config
	@cd src; make $@

scrub: config
	@cd src; make $@

patchable: config
	@cd src; make $@

install: config
	@cd src; make $@

release:
	@etc/makerelease

commit:
	@cd src; make commitable
	cvs commit

config:
	@if test ! -f src/Makefile; then ./configure; fi
