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

mod_LTLIBRARIES=guile.la

guile_la_SOURCES = guile.c
guile_la_LIBADD = ../../lib/libsmap.la @GUILE_LIBS@
AM_LDFLAGS = -module -avoid-version -no-undefined
AM_CPPFLAGS = -I$(top_srcdir)/include @GUILE_INCLUDES@

SUFFIXES =
BUILT_SOURCES =
EXTRA_DIST = guile.x getpw.scm
DOT_X_FILES = guile.x
CLEANFILES=
DISTCLEANFILES=
MAINTAINERCLEANFILES=
sitedir = @GUILE_SITE@/$(PACKAGE)
site_DATA=getpw.scm

AM_CPPFLAGS += -DADDLOADPATH=\"$(sitedir)\"

include ../../gint/gint.mk
