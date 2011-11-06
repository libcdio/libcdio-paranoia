####################################################
# Things for regression testing
####################################################
#
#
# There's a problem with doing make distcheck for testdefault. 
# A reminder of why I hate automake.

SUBDIRS = data

testparanoia=testparanoia
testparanoia_LDADD = $(LIBCDIO_PARANOIA_LIBS) $(LIBCDIO_CDDA_LIBS) $(LIBCDIO_LIBS) $(LTLIBICONV) -lrt

hack = $(testparanoia) 

EXTRA_PROGRAMS = testdefault 
DATA_DIR       = @abs_top_srcdir@/test/data

INCLUDES = -I$(top_srcdir) $(LIBCDIO_CFLAGS) $(LIBCDIO_PARANOIA_CFLAGS)

check_SCRIPTS = check_paranoia.sh
# If we beefed this up so it checked to see if a CD-DA was loaded
# it could be an automatic test. But for now, not so.
#               check_paranoia.sh

check_PROGRAMS = testparanoia

check_DATA = cd-paranoia-log.right 

EXTRA_DIST = $(check_SCRIPTS) $(check_DATA)

TESTS = $(check_PROGRAMS) $(check_SCRIPTS) 

MOSTLYCLEANFILES = core core.* *.dump cdda-orig.wav cdda-try.wav *.raw

test: check-am
