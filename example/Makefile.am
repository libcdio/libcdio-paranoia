####################################################
# Sample programs
####################################################
#
if ENABLE_CPP
SUBDIRS =  C++
endif

noinst_PROGRAMS = paranoia paranoia2

AM_CPPFLAGS = -I$(top_srcdir) $(LIBCDIO_PARANOIA_CFLAGS) $(LIBCDIO_CFLAGS)

paranoia_LDADD  = $(LIBCDIO_PARANOIA_LIBS) $(LIBCDIO_CDDA_LIBS) $(LIBCDIO_LIBS) $(LTLIBICONV)
paranoia2_LDADD = $(LIBCDIO_PARANOIA_LIBS) $(LIBCDIO_CDDA_LIBS) $(LIBCDIO_LIBS) $(LTLIBICONV)

check_PROGRAMS      = $(noinst_PROGRAMS)
TESTS               = $(check_PROGRAMS)

# iso programs create file "copying"
MOSTLYCLEANFILES = copying *.wav
