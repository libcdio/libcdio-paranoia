#   Copyright (C) 2011, 2012, 2015, 2020, 2024
#   Rocky Bernstein <rocky@gnu.org>
#
## Process this file with automake to produce Makefile.in
##   which configure then turns into a Makefile  ...
##     which make can then use to produce stuff. Isn't configuration simple?

AUTOMAKE_OPTIONS = dist-bzip2

EXTRA_DIST = \
	THANKS \
        NEWS.md \
        INSTALL.md \
        README.md \
	make-check-filter.rb \
	example/README \
	COPYING \
	$(wildcard test/data/*.bin) \
	$(wildcard test/data/*.cue) \
	$(wildcard test/data/*.iso) \
	$(wildcard test/*.right)

SUBDIRS = doc include lib src test example

paranoiapcs = libcdio_paranoia.pc libcdio_cdda.pc

# Note that README and INSTALL are not made by
# default nor should the need to be
#: README text from Markdown
README: README.md
	pandoc -f markdown -t plain --wrap=none $< -o $@

#: INSTALL text from Markdown
INSTALL: INSTALL.md
	pandoc -f markdown -t plain --wrap=none $< -o $@

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
