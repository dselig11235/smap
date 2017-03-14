# This file is part of Smap. 
# Copyright (C) 2010, 2014 Sergey Poznyakoff
#
# Smap is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# Smap is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Smap.  If not, see <http://www.gnu.org/licenses/>.

moddir=@SMAP_MODDIR@

mod_LTLIBRARIES=mailutils.la

mailutils_la_SOURCES = mailutils.c
mailutils_la_LIBADD = ../../lib/libsmap.la @MAILUTILS_LIBS@
AM_LDFLAGS = -module -avoid-version -no-undefined
AM_CPPFLAGS = -I$(top_srcdir)/include @MAILUTILS_INCLUDES@
