# This file is part of Smap.
# Copyright (C) 2010, 2014, 2015 Sergey Poznyakoff
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

info_TEXINFOS=smap.texi
smap_TEXINFOS=\
 fdl.texi\
 sockmap.texi\
 debugcat.texi\
 ex-meta1.texi\
 macros.texi

EXTRA_DIST = \
 gendocs_template\
 gendocs.sh\
 mastermenu.el\
 untabify.el\
 smapflow.txt\
 smapflow.fig\
 smapflow.png\
 smapflow.eps

clean-local:
	@rm -rf manual

# Checking
check-format:
	@if test -n "`cat $(info_TEXINFOS) $(smap_TEXINFOS) | tr -d -c '\t'`"; then \
		echo "Sources contain tabs; run make untabify"; \
		false; \
	fi

all-check-docs: check-format

check-docs:
	@$(MAKE) -k all-check-docs

#

master-menu:
	$(AM_V_GEN)emacs -batch -l mastermenu.el -f make-master-menu $(info_TEXINFOS)

untabify:
	@emacs -batch -l untabify.el $(info_TEXINFOS) $(mailfromd_TEXINFOS)

final: untabify master-menu

MAKEINFOFLAGS=

GENDOCS=$(srcdir)/gendocs.sh --no-copy-images --html '--init-file=$(abs_srcdir)/html.init'
EXTRA_DIST += gendocs.sh

TEXI2DVI=texi2dvi -t '@set $(RENDITION)'

# Make sure you set TEXINPUTS.
# TEXINPUTS=/usr/share/texmf/pdftex/plain/misc/ is ok for most distributions
.PHONY: manual
manual:
	TEXINPUTS=$(srcdir):$(top_srcdir)/build-aux:$(TEXINPUTS) \
	 MAKEINFO="$(MAKEINFO) $(MAKEINFOFLAGS)" \
	 TEXI2DVI="$(TEXI2DVI) -t @finalout" \
	 $(GENDOCS) $(PACKAGE) '$(PACKAGE_NAME) manual'

manual.tar.bz2: manual
	tar cf manual.tar manual
	tar -r -f manual.tar --transform 's|^|graphics/|' smapflow.png
	bzip2 -f manual.tar

man-tar: manual.tar.bz2
