#   Copyright (C) 2011, 2012, 2015
#   Rocky Bernstein <rocky@gnu.org>
#
## Process this file with automake to produce Makefile.in
##   which configure then turns into a Makefile  ...
##     which make can then use to produce stuff. Isn't configuration simple?

AUTOMAKE_OPTIONS = dist-bzip2

EXTRA_DIST = \
	THANKS \
        README.md \
	make-check-filter.rb \
	example/README \
	COPYING

SUBDIRS = doc include lib src test example

paranoiapcs = libcdio_paranoia.pc libcdio_cdda.pc

# pkg-config(1) related rules
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcdio_paranoia.pc libcdio_cdda.pc

$(pkgconfig_DATA): config.status

#: run regression tests
test: check

#: Run all tests without bloated output
check-short:
	$(MAKE) check 2>&1  | ruby @abs_top_srcdir@/make-check-filter.rb

#: Make documentation via Doxygen http://www.stack.nl/~dimitri/doxygen/
doxygen:
	-( cd ${top_srcdir}/doc/doxygen && /bin/sh ${srcdir}/run_doxygen )

MAINTAINERCLEANFILES = ChangeLog *.rej *.orig

if MAINTAINER_MODE

.PHONY: ChangeLog
#: Create ChangeLog from version control
ChangeLog:
	git log --pretty --numstat --summary | $(GIT2CL) >$@

ACLOCAL_AMFLAGS=-I m4

endif
