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

lib_LTLIBRARIES = libsmap.la

libsmap_la_SOURCES = \
 asnprintf.c\
 asprintf.c\
 debug.c\
 diag.c\
 fileoutstr.c\
 kwtab.c\
 sockmapstr.c\
 parseopt.c\
 progname.c\
 stderr.c\
 stream.c\
 stream_printf.c\
 stream_vprintf.c\
 syslog.c\
 syslogstr.c\
 tracestr.c\
 url.c\
 vasnprintf.c\
 wordsplit.c\
 xscript.c

libsmap_la_LDFLAGS = -version-info 0:0:0

AM_CPPFLAGS = -I$(top_srcdir)/include
