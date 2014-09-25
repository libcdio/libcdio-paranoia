####################################################
# lib/cdda_interface regression testing
####################################################

AM_CPPFLAGS = $(LIBCDIO_CFLAGS) $(LIBISO9660_CFLAGS) $(LIBCDIO_PARANOIA_CFLAGS)
DATA_DIR = $(abs_top_srcdir)/test/data

toc_SOURCES      = toc.c
toc_LDADD        = $(LIBCDIO_PARANOIA_LIBS) $(LIBCDIO_CDDA_LIBS) $(LIBCDIO_LIBS) $(LTLIBICONV)
toc_CFLAGS       = -DDATA_DIR=\"$(DATA_DIR)\"

check_PROGRAMS   = \
	toc

TESTS = $(check_PROGRAMS)

EXTRA_DIST = \
	toc.c.in

MOSTLYCLEANFILES = \
	$(check_PROGRAMS) \
	core core.* *.dump cdda-orig.wav cdda-try.wav *.raw

#: run regression tests. "test" is the same thing as "check"
test: check-am

#: Run all tests without bloated output
check-short:
	$(MAKE) check 2>&1  | ruby @abs_top_srcdir@/make-check-filter.rb
