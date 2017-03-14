/* This file is part of Smap.
   Copyright (C) 2006-2007, 2010, 2014 Sergey Poznyakoff

   Smap is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Smap is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Smap.  If not, see <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include "smap/stream.h"
#include "smap/streamdef.h"

struct syslog_stream {
	struct _smap_stream base;
	int prio;
	char *pfx;
};

static int
_syslog_stream_write(struct _smap_stream *stream, const char *buf,
		      size_t size, size_t *pret)
{
	struct syslog_stream *lsp = (struct syslog_stream *)stream;

	*pret = size;
	if (size > 0 && buf[size-1] == '\n')
		size--;
	if (size == 0)
		return 0;
	if (lsp->pfx) {
		syslog(lsp->prio, "[%s] %*.*s", lsp->pfx, (int) size,
		       (int) size, buf);
	} else
		syslog(lsp->prio, "%*.*s", (int) size, (int) size, buf);
	return 0;
}

static void
_syslog_stream_destroy(struct _smap_stream *stream)
{
	struct syslog_stream *lsp = (struct syslog_stream *)stream;
	free(lsp->pfx);
}

int
smap_syslog_stream_create(smap_stream_t *pstream, int prio,
			  const char *pfx)
{
	struct syslog_stream *str =
		(struct syslog_stream *)
		  _smap_stream_create(sizeof(*str), SMAP_STREAM_WRITE);
	if (!str)
		return ENOMEM;
	if (pfx) {
		str->pfx = strdup(pfx);
		if (!str->pfx) {
			free(str);
			return ENOMEM;
		}
	} else
		str->pfx = NULL;
	str->prio = prio;
	str->base.write = _syslog_stream_write;
	str->base.done = _syslog_stream_destroy;
	*pstream = (smap_stream_t) str;
	smap_stream_set_buffer(*pstream, smap_buffer_line, 1024);
	return 0;
}
