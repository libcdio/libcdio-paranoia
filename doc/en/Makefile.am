#   $Id: Makefile.am,v 1.3 2008/04/17 17:39:48 karl Exp $
#
#   Copyright (C) 2005, 2008 Rocky Bernstein <rocky@gnu.org>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program; if not, write to the Free Software Foundation, Inc.,
#   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

manfiles = cd-paranoia.1 
man_MANS = $(manfiles)
transform = s,cd-paranoia,@CDPARANOIA_NAME@,

EXTRA_DIST = $(manfiles) cd-paranoia.1.in

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in $(manfiles)
