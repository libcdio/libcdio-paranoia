#   Copyright (C) 2005, 2006, 2008, 2009 Rocky Bernstein <rocky@gnu.org>
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.

##########################################################
# Sample C++ programs using libcdio (without OO wrapper)
#########################################################
#
paranoia_progs = paranoia paranoia2
if BUILD_EXAMPLES
noinst_PROGRAMS = $(paranoia_progs)
endif
AM_CPPFLAGS = -I$(top_srcdir) $(LIBCDIO_CFLAGS)

paranoia_SOURCES  = paranoia.cpp
paranoia_LDADD    = $(LIBCDIO_PARANOIA_LIBS) $(LIBCDIO_CDDA_LIBS) \
	            $(LIBCDIO_LIBS)
paranoia2_SOURCES = paranoia.cpp
paranoia2_LDADD   = $(LIBCDIO_PARANOIA_LIBS) $(LIBCDIO_CDDA_LIBS) \
	            $(LIBCDIO_LIBS)

check_PROGRAMS      = $(noinst_PROGRAMS)

TESTS               = $(check_PROGRAMS)

# iso programs create file "copying"
MOSTLYCLEANFILES = copying *.wav
