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
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "smap/diag.h"
#include "smap/stream.h"
#include "smap/streamdef.h"

struct fileout_stream {
	struct _smap_stream base;
	FILE *file;
	char *pfx;
	int pgopt;
};

static int
_fileout_stream_write(struct _smap_stream *stream, const char *buf,
		      size_t size, size_t *pret)
{
	struct fileout_stream *str = (struct fileout_stream *)stream;
	size_t n;

	if (str->pgopt)
		fprintf(str->file, "%s: ", smap_progname);
	if (str->pfx)
		fprintf(str->file, "%s: ", str->pfx);
	n = fwrite(buf, 1, size, str->file);
	*pret = n;
	if (n != size)
		return errno;
	return 0;
}

static int
_fileout_stream_flush(struct _smap_stream *stream)
{
	struct fileout_stream *str = (struct fileout_stream *)stream;
	if (fflush(str->file))
		return errno;
	return 0;
}

static void
_fileout_stream_destroy(struct _smap_stream *stream)
{
	struct fileout_stream *str = (struct fileout_stream *)stream;
	free(str->pfx);
}

int
smap_fileout_stream_create (smap_stream_t *pstream, FILE *file,
			    const char *pfx, int pgopt)
{
	struct fileout_stream *str =
		(struct fileout_stream *)
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
	str->file = file;
	str->pgopt = pgopt;
	str->base.write = _fileout_stream_write;
	str->base.flush = _fileout_stream_flush;
	str->base.done = _fileout_stream_destroy;
	*pstream = (smap_stream_t) str;
	smap_stream_set_buffer(*pstream, smap_buffer_line, 1024);
	return 0;
}
