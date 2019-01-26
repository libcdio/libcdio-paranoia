dnl Copyright (C)  2011-2017, 2019
dnl Rocky Bernstein <rocky@gnu.org>
dnl
dnl This program is free software: you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation, either version 3 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.

define(RELEASE_NUM, 2.0.0)
define(PARANOIA_VERSION, 10.2)
define(LIBCDIO_PARANOIA_VERSION_STR, $1+$2)

AC_DEFINE([PACKAGE], [libcdio-paranoia], [cdparanoia using libcdio])

AC_PREREQ(2.67)

AC_INIT([libcdio-paranoia], [LIBCDIO_PARANOIA_VERSION_STR(PARANOIA_VERSION,RELEASE_NUM)], [libcdio-help@gnu.org])
AC_CONFIG_SRCDIR(src/cd-paranoia.c)
AM_INIT_AUTOMAKE([foreign])

AC_CANONICAL_HOST
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_MISSING_PROG(HELP2MAN, help2man, $missing_dir)
AM_MISSING_PROG(GIT2CL, git2cl, $missing_dir)
AM_MAINTAINER_MODE

AM_SANITY_CHECK

AC_ARG_WITH(cd-paranoia-name,
	AC_HELP_STRING([--with-cd-paranoia-name], [name to use as the cd-paranoia program name (default cd-paranoia)]),
cd_paranoia_name="${withval}", cd_paranoia_name="cd-paranoia")
CDPARANOIA_NAME="$cd_paranoia_name"
AC_SUBST(CDPARANOIA_NAME)

AC_ARG_WITH(versioned-libs,
	AC_HELP_STRING([--without-versioned-libs], [build versioned library symbols (default enabled if you have GNU ld)]),
enable_versioned_libs="${withval}", enable_versioned_libs=yes)

AC_ARG_ENABLE(example-progs,
	AC_HELP_STRING([--disable-example-progs], [Don't build libcdio-paranoia sample programs]))

AC_ARG_ENABLE([cxx],
	AC_HELP_STRING([--disable-cxx], [Disable C++ bindings (default enabled)]))
AM_CONDITIONAL([ENABLE_CXX_BINDINGS], [test "x$enable_cxx" != "xno"])

AC_ARG_ENABLE(cpp-progs,
	AC_HELP_STRING([--enable-cpp-progs], [make C++ example programs (default enabled)]))
AM_CONDITIONAL(ENABLE_CPP, test x"$enable_cpp_progs" = "xyes")

dnl cheeck whether ld supports --version-script

AC_ARG_ENABLE(ld-version-script,
  AS_HELP_STRING([--disable-ld-version-script],
  [Disable linker version script for libraries (Defauilt is to use linker script if the linger supports it)]),
  [enable_version_script=$enableval],
  [enable_version_script=yes])

AM_CONDITIONAL(VERSION_SCRIPT, test "x$enable_version_script" = "xyes")


# Do we have GNU ld? If we don't, we can't build versioned symbols.
AC_ARG_ENABLE(example-progs,
	AC_HELP_STRING([--disable-example-progs], [Don't build libcdio sample programs]))

AM_CONDITIONAL(BUILD_EXAMPLES, test "x$enable_example_progs" != "xno")
AM_CONDITIONAL(BUILD_VERSIONED_LIBS, test "x$enable_versioned_libs" = "xyes")

dnl We use C
AC_PROG_CC
AM_PROG_CC_C_O
dnl We also use C++ in example programs and for CXX bindings
AC_PROG_CXX

dnl Checks for programs.
AC_AIX

AC_PROG_MKDIR_P

AC_PROG_AWK
AC_SUBST(AWK)

if test "x$GCC" != "xyes"
then
  AC_MSG_WARN([
  *** non GNU CC compiler detected.
  *** This package has not been tested very well with non GNU compilers
  *** you should try to use 'gcc' for compiling this package.])
else
  WARN_CFLAGS="-Wall -Wchar-subscripts -Wmissing-prototypes -Wmissing-declarations -Wunused -Wpointer-arith -Wwrite-strings -Wnested-externs -Wno-sign-compare"

  for WOPT in $WARN_CFLAGS; do
    SAVE_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $WOPT"
    AC_MSG_CHECKING([whether $CC understands $WOPT])
    AC_TRY_COMPILE([], [], has_option=yes, has_option=no,)
    CFLAGS="$SAVE_CFLAGS"
    AC_MSG_RESULT($has_option)
    if test $has_option = yes; then
      warning_flags="$warning_flags $option"
    fi
    unset has_option
    unset SAVE_CFLAGS
  done
  WARNING_FLAGS="$warning_flags"
  unset warning_flags
fi

PKG_CHECK_MODULES(LIBCDIO, libcdio >= 0.83, [],
	[AC_MSG_ERROR(Required libcdio library not found. Please get libcdio from http://www.gnu.org/software/libcdio/ and install it.)])
AC_SUBST(LIBCDIO_LIBS)
AC_SUBST(LIBCDIO_CFLAGS)

# We use Perl for documentation and regression tests
AC_PATH_PROG(PERL, perl, false)
AC_SUBST(PERL)
AM_CONDITIONAL(HAVE_PERL, test "$PERL" != "false")

# We use a diff in regression testing
AC_PATH_PROG(DIFF, diff, no)
DIFF_OPTS=

if test "$DIFF" = no ; then
   AC_PATH_PROG(DIFF, cmp, no)
else
  # Try for GNU diff options.
  # MSDOG output uses \r\n rather than \n in tests
  for diff_opt in -w --unified ; do
    if $DIFF $diff_opt ./configure ./configure > /dev/null 2>&1; then
      AC_MSG_RESULT([adding $diff_opt to diff in regression tests])
      DIFF_OPTS="$DIFF_OPTS $diff_opt"
    fi
  done
fi
AC_SUBST(DIFF)
AC_SUBST(DIFF_OPTS)

# We use cmp, dd, and wc in cd-paranoia regression testing
AC_PATH_PROG([CMP], [cmp], [no])
AC_SUBST([CMP])

AC_PATH_PROG([DD], [dd], [no])
AC_SUBST([DD])

AC_PATH_PROG([WC], [wc], [no])
AC_SUBST([WC])

dnl headers

AC_HEADER_STDC
AC_CHECK_HEADERS(assert.h errno.h fcntl.h glob.h limits.h pwd.h)
AC_CHECK_HEADERS(stdarg.h stdbool.h stdio.h sys/cdio.h sys/param.h \
		 sys/time.h sys/timeb.h sys/utsname.h)

# FreeBSD 4 has getopt in unistd.h. So we include that before
# getopt.h
AC_CHECK_HEADERS(unistd.h getopt.h)

AC_SUBST(SBPCD_H)
AC_SUBST(TYPESIZES)

dnl compiler
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE

dnl ISOC99_PRAGMA
AC_MSG_CHECKING([whether $CC supports ISOC99 _Pragma()])
AC_TRY_COMPILE([],[_Pragma("pack(1)")], [
  ISOC99_PRAGMA=yes
  AC_DEFINE(HAVE_ISOC99_PRAGMA, [], [Supports ISO _Pragma() macro])
],ISOC99_PRAGMA=no)
AC_MSG_RESULT($ISOC99_PRAGMA)

##
## Check for S_ISSOCK() and S_ISLNK() macros
##
AC_MSG_CHECKING(for S_ISLNK() macro)
AC_LINK_IFELSE([AC_LANG_PROGRAM([
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
],[return S_ISLNK(0);])],
[ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_S_ISLNK, [], [Define this <sys/stat.h> defines S_ISLNK()]) ],
[ AC_MSG_RESULT(no) ])

AC_MSG_CHECKING([for S_ISSOCK() macro])
AC_LINK_IFELSE([AC_LANG_PROGRAM([
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
],[return S_ISSOCK(0);])],
[ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_S_ISSOCK, [], [Define this <sys/stat.h> defines S_ISSOCK()]) ],
[ AC_MSG_RESULT(no) ])

AC_MSG_CHECKING([for struct timespec])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
],[struct timespec ts;])],
[ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_STRUCT_TIMESPEC, [], [Define this if you have struct timespec]) ],
[ AC_MSG_RESULT(no) ])

dnl empty_array_size
AC_MSG_CHECKING([how to create empty arrays])

empty_array_size="xxxx"
AC_TRY_COMPILE([],[struct { int foo; int bar[]; } doo;], empty_array_size="")

if test "x$empty_array_size" = "xxxx";then
 AC_TRY_COMPILE([],[struct { int foo; int bar[0]; } doo;], empty_array_size="0")
fi

if test "x$empty_array_size" = "xxxx"
then
  AC_MSG_ERROR([compiler is unable to creaty empty arrays])
else
  AC_DEFINE_UNQUOTED(EMPTY_ARRAY_SIZE, $empty_array_size,
	[what to put between the brackets for empty arrays])
  changequote(`,')
  msg="[${empty_array_size}]"
  changequote([,])
  AC_MSG_RESULT($msg)
fi

dnl Enable the creation of shared libraries under Win32.
AC_LIBTOOL_WIN32_DLL

dnl AM_PROG_LIBTOOL tests whether we have GNU ld
dnl this must come before checking --with-versioned-libs
dnl which requires GNU ld.
AM_PROG_LIBTOOL

dnl regression tests need native-style absolute paths.
dnl native_abs_top_srcdir is used here.
case $srcdir in
  .)  # We are building in place.
   native_abs_top_srcdir=$ac_pwd ;;
  changequote(`,')
  [\\/]* | ?:[\\/]* )  # Absolute name.
  changequote([,])
    native_abs_top_srcdir=$srcdir ;;
  *) # Relative name.
    native_abs_top_srcdir=$ac_pwd/$srcdir ;;
esac
AC_SUBST(native_abs_top_srcdir)

dnl system

# FIXME:
# I believe some OS's require -lm, but I don't recall for what function
# When we find it, put it in below instead of "cos".
AC_CHECK_LIB(m, cos, [LIBS="$LIBS -lm"; COS_LIB="-lm"])
CFLAGS="$CFLAGS $WARN_CFLAGS"
AC_SUBST(COS_LIB)

# Linux has clock_gettime in librt
AC_CHECK_LIB(rt, clock_gettime)

if test "$with_gnu_ld" != yes; then
   AC_MSG_WARN([I don't see GNU ld. I'm going to assume --without-versioned-libs])
   enable_versioned_libs='no'
fi
# We also need GNU make to build versioned symbols.
if test "x$enable_versioned_libs" = "xyes" ; then
  if test "x$enable_version_script" = "xyes"; then
    if test -n "$MAKE" ; then
      $MAKE --version 2>/dev/null >/dev/null
      if test "$?" -ne 0 ; then
         AC_MSG_ERROR(Either set MAKE variable to GNU make or use
     --without-versioned-libs option)
      fi
    else
      make --version 2>/dev/null >/dev/null
      if test "$?" -ne 0 ; then
         AC_MSG_ERROR(Either set MAKE variable to GNU make or use
     --without-versioned-libs option)
      fi
    fi
  else
    make --version 2>/dev/null >/dev/null
    if test "$?" -ne 0 ; then
       AC_MSG_ERROR(Either set MAKE variable to GNU make or use
   --without-versioned-libs option)
    fi
  fi
fi

AM_CONDITIONAL(CYGWIN, test "x$CYGWIN" = "xyes")

dnl Checks for header files.

LIBCDIO_CDDA_LIBS='$(top_builddir)/lib/cdda_interface/libcdio_cdda.la'
LIBCDIO_PARANOIA_CFLAGS='-I$(top_builddir)/include -I$(top_srcdir)/include/'
LIBCDIO_PARANOIA_LIBS='$(top_builddir)/lib/paranoia/libcdio_paranoia.la'
AC_SUBST(LIBCDIO_CDDA_LIBS)
AC_SUBST(LIBCDIO_PARANOIA_CFLAGS)
AC_SUBST(LIBCDIO_PARANOIA_LIBS)

dnl Libtool flag for strict linkage
LT_NO_UNDEFINED=

case $host_os in
     aix*)
       ## Don't use AIX driver until starts to really work
       ## cd_drivers="${cd_drivers}, AIX"
       ## AC_DEFINE([HAVE_AIX_CDROM], [1],
       ##     [Define 1 if you have AIX CD-ROM support])
       ;;
     darwin[[6-9]].*|darwin1[[0-4]].*)
       AC_CHECK_HEADERS(IOKit/IOKitLib.h CoreFoundation/CFBase.h,
                        [have_iokit_h="yes"])
       if test "x$have_iokit_h" = "xyes" ; then
         AC_DEFINE([HAVE_DARWIN_CDROM], [1],
	      [Define 1 if you have Darwin OS X-type CD-ROM support])
	 DARWIN_PKG_LIB_HACK="-Wl,-framework,CoreFoundation -Wl,-framework,IOKit"

	 dnl Prior to Mac OS X 10.4 (Tiger), DiskArbitration was a private framework.
	 dnl It's now public, and it's needed to do cd/dvd unmount/eject.
	 AC_MSG_CHECKING([for DiskArbitration framework])
	 ac_save_LIBS="$LIBS" LIBS="$LIBS -framework CoreFoundation -framework DiskArbitration"
	 AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <DiskArbitration/DiskArbitration.h>]], [[]])],
	      [have_diskarbitration_framework=yes], [have_diskarbitration_framework=no])
	 LIBS="$ac_save_LIBS"
	 AC_MSG_RESULT([$have_diskarbitration_framework])
	 if test x"$have_diskarbitration_framework" = x"yes"; then
	     AC_DEFINE([HAVE_DISKARBITRATION], 1, [Define to 1 if you have the Apple DiskArbitration framework])
	     DARWIN_PKG_LIB_HACK="$DARWIN_PKG_LIB_HACK -Wl,-framework,DiskArbitration"
	 fi

	 AC_SUBST(DARWIN_PKG_LIB_HACK)
	 LIBCDIO_LIBS="$LIBCDIO_LIBS $DARWIN_PKG_LIB_HACK"
         cd_drivers="${cd_drivers}, Darwin"
       fi
     ;;
     linux*|uclinux)
        AC_CHECK_HEADERS(linux/version.h linux/major.h)
        AC_CHECK_HEADERS(linux/cdrom.h, [have_linux_cdrom_h="yes"])
	if test "x$have_linux_cdrom_h" = "xyes" ; then
	   AC_TRY_COMPILE(,[
#include <linux/cdrom.h>
struct cdrom_generic_command test;
int has_timeout=sizeof(test.timeout);],
            [AC_DEFINE([HAVE_LINUX_CDROM_TIMEOUT], [1],
                   [Define 1 if timeout is in cdrom_generic_command struct])])
	   AC_DEFINE([HAVE_LINUX_CDROM], [1],
                     [Define 1 if you have Linux-type CD-ROM support])
	   cd_drivers="${cd_drivers}, GNU/Linux"
	fi
	;;
     bsdi*)
        AC_CHECK_HEADERS(dvd.h, [have_bsdi_dvd_h="yes"])
	if test "x$have_bsdi_dvd_h" = "xyes" ; then
     	  AC_DEFINE([HAVE_BSDI_CDROM], [1],
                    [Define 1 if you have BSDI-type CD-ROM support])
	 LIBS="$LIBS -ldvd -lcdrom"
         LIBCDIO_LIBS="$LIBCDIO_LIBS  -lcdrom"
	  cd_drivers="${cd_drivers}, BSDI"
	fi
	;;
     sunos*|sun*|solaris*)
     	AC_DEFINE([HAVE_SOLARIS_CDROM], [1],
                  [Define 1 if you have Solaris CD-ROM support])
	  cd_drivers="${cd_drivers}, Solaris"
	;;
     cygwin*)
	AC_DEFINE([CYGWIN], [1],
                  [Define 1 if you are compiling using cygwin])
     	AC_DEFINE([HAVE_WIN32_CDROM], [1],
                  [Define 1 if you have MinGW CD-ROM support])
 	LIBS="$LIBS -lwinmm"
 	LT_NO_UNDEFINED="-no-undefined"
  	cd_drivers="${cd_drivers}, MinGW"
     	AC_DEFINE([NEED_TIMEZONEVAR], [1],
                  [Define 1 if you need timezone defined to get timzone
		   defined as a variable. In cygwin it is a function too])
	;;
     mingw*)
	AC_CHECK_HEADERS(windows.h)
	AC_DEFINE([MINGW32], [1],
                  [Define 1 if you are compiling using MinGW])
     	AC_DEFINE([HAVE_WIN32_CDROM], [1],
                  [Define 1 if you have MinGW CD-ROM support])
 	LIBS="$LIBS -lwinmm"
 	LT_NO_UNDEFINED="-no-undefined"
  	cd_drivers="${cd_drivers}, MinGW "
	;;
     freebsd[[4-9]].*|freebsd10.*|dragonfly*|kfreebsd*)
     	AC_DEFINE([HAVE_FREEBSD_CDROM], [1],
                  [Define 1 if you have FreeBSD CD-ROM support])
 	LIBS="$LIBS -lcam"
  	cd_drivers="${cd_drivers}, FreeBSD "
	;;
     netbsd*|openbsd*)
     	AC_DEFINE([HAVE_NETBSD_CDROM], [1],
                  [Define 1 if you have NetBSD CD-ROM support])
 	# LIBS="$LIBS -lcam"
  	cd_drivers="${cd_drivers}, NetBSD "
	;;
     os2*)
       AC_DEFINE([HAVE_OS2_CDROM], [1],
                  [Define 1 if you have OS/2 CD-ROM support])
       LT_NO_UNDEFINED="-no-undefined"
       LDFLAGS="$LDFLAGS -Zbin-files"
       cd_drivers="${cd_drivers}, OS2 "
       ;;
     *)
	AC_MSG_WARN([Don't have OS CD-reading support for ${host_os}...])
	AC_MSG_WARN([Will use generic support.])
	;;
esac

AC_SUBST(LT_NO_UNDEFINED)
AC_SUBST(DARWIN_PKG_LIB_HACK)

LIBCDIO_PARANOIA_SOURCE_PATH="`pwd`"
AC_DEFINE_UNQUOTED(LIBCDIO_PARANOIA_SOURCE_PATH, "$LIBCDIO_SOURCE_PATH",
	[Full path to libcdio-paranoia top_sourcedir.])
AC_SUBST(LIBCDIO_PARANOIA_SOURCE_PATH)

AC_CHECK_FUNCS( [bzero chdir drand48 ftruncate geteuid getgid \
		 getuid getpwuid gettimeofday lstat memcpy memset \
		 rand seteuid setegid snprintf setenv unsetenv tzset \
		 sleep usleep vsnprintf readlink realpath gmtime_r \
		 localtime_r clock_gettime] )

dnl
dnl Output configuration files
dnl
## AC_CONFIG_FILES([  po/Makefile.in\
AC_CONFIG_FILES([
       Makefile \
       doc/Makefile \
       doc/doxygen/Doxyfile \
       doc/en/Makefile \
       doc/en/cd-paranoia.1 \
       doc/ja/Makefile \
       doc/ja/cd-paranoia.1 \
       example/Makefile \
       example/C++/Makefile \
       include/Makefile \
       include/cdio/Makefile \
       include/cdio/paranoia/Makefile \
       include/cdio/paranoia/version.h \
       lib/Makefile \
       lib/cdda_interface/Makefile \
       lib/paranoia/Makefile \
       libcdio_cdda.pc \
       libcdio_paranoia.pc \
       src/Makefile \
       src/usage.txt \
       test/data/Makefile
       test/cdda_interface/Makefile
       test/cdda_interface/toc.c
       test/Makefile
       ])

# AC_CONFIG_FILES([po/Makefile])
AC_CONFIG_FILES([test/check_paranoia.sh], [chmod +x test/check_paranoia.sh])
AC_CONFIG_FILES([test/endian.sh], [chmod +x test/endian.sh])
AC_CONFIG_FILES([test/check_start_track_not_one.sh], [chmod +x test/check_start_track_not_one.sh])
AC_OUTPUT

AC_MSG_NOTICE([
Building C++ programs: $(test "x$enable_cxx" != "xno" && echo yes || echo no)])
