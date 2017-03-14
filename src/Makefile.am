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

sbin_PROGRAMS = smapd
bin_PROGRAMS = smapc

smapd_SOURCES = \
 cfg.c\
 close-fds.c\
 log.c\
 mem.c\
 module.c\
 smapd.c\
 srvman.c\
 userprivs.c\
 query.c

smapd_LDADD = ../lib/libsmap.la @TCPWRAP_LIBRARIES@ @LIBLTDL@

noinst_HEADERS = smapd.h srvman.h common.h

smapc_SOURCES = smapc.c mem.c
smapc_LDADD = ../lib/libsmap.la @READLINE_LIBS@

SUFFIXES=.opt .c

.opt.c:
	m4 -s $(top_srcdir)/src/getopt.m4 $< | sed '1d' > $@

BUILT_SOURCES=cmdline.c smapccmd.c smapd.conf
EXTRA_DIST=cmdline.opt cmdline.c smapccmd.opt smapccmd.c smapd.cfin getopt.m4
AM_CPPFLAGS =\
 -I$(top_srcdir)/include @LTDLINCL@\
 -DSYSCONFDIR=\"$(sysconfdir)\"\
 -DSMAP_MODDIR=\"$(SMAP_MODDIR)\"
CLEANFILES = smapd.conf

cmdline.c: cmdline.opt getopt.m4
smapccmd.c: smapccmd.opt getopt.m4

if MAILUTILS_COND
  DEF_MAILUTILS=-DMAILUTILS
endif
if GUILE_COND
  DEF_GUILE=-DGUILE
endif

.cfin.conf:
	$(AM_V_GEN)m4 $(DEF_MAILUTILS) $(DEF_GUILE) $< > $@

install-data-local:
	@test -z "$(sysconfdir)" || $(mkdir_p) "$(DESTDIR)$(sysconfdir)"
	@if [ -r $(DESTDIR)$(sysconfdir)/smapd.conf ]; then :; \
	else ${INSTALL} -m 644 $(top_builddir)/src/smapd.conf \
               $(DESTDIR)$(sysconfdir)/smapd.conf; fi

