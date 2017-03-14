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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "smap/stream.h"
#include "smap/streamdef.h"

struct tracepfx {
	char *str;
	size_t len;
};

struct trace_stream {
	struct _smap_stream base;
	struct tracepfx *pfxv;
	size_t pfxc;
	smap_stream_t transport;
};

static const char *
find_resp_marker(const char *buf, size_t size)
{
	while (size) {
		const char *p = memchr(buf, '=', size);
		if (!p)
			return NULL;
		if (p[1] == '>') {
			for (p += 2; *p && (*p == ' ' || *p == '\t'); p++)
				;
			return p;
		}
		size -= p - buf + 1;
		buf = p + 1;
	}
	return NULL;
}

static void
_trace_free_pfx(struct trace_stream *tp)
{
	size_t i;

	for (i = 0; i < tp->pfxc; i++)
		free(tp->pfxv[i].str);
	free(tp->pfxv);
	tp->pfxc = 0;
	tp->pfxv = NULL;
}

static int
_trace_new_pfx(struct trace_stream *tp, char **args)
{
	size_t i, count;

	for (i = 0; args[i]; i++)
		;
	count = i;
	tp->pfxv = calloc(count, sizeof(tp->pfxv[0]));
	if (!tp->pfxv)
		return errno;
	for (i = 0; i < count; i++) {
		if (!(tp->pfxv[i].str = strdup(args[i]))) {
			tp->pfxc = i;
			_trace_free_pfx(tp);
			return ENOMEM;
		}
		tp->pfxv[i].len = strlen(args[i]);
	}
	tp->pfxc = count;
	return 0;
}

static int
match_response(struct trace_stream *tp, const char *resp, size_t len)
{
	size_t i;

	for (i = 0; i < tp->pfxc; i++) {
		if (tp->pfxv[i].len <= len
		    && memcmp(resp, tp->pfxv[i].str, tp->pfxv[i].len) == 0)
			return 1;
	}
	return 0;
}

static int
_trace_write(struct _smap_stream *stream, const char *buf,
	     size_t size, size_t *pret)
{
	struct trace_stream *tp = (struct trace_stream *) stream;
	const char *p;

	if (tp->pfxc
	    && (p = find_resp_marker(buf, size))
	    && !match_response(tp, p, size - (p - buf))) {
		*pret = size;
		return 0;
	}
	return smap_stream_write(tp->transport, buf, size, pret);
}

static int
_trace_close(struct _smap_stream *stream)
{
	struct trace_stream *tp = (struct trace_stream *) stream;
	if (stream->flags & SMAP_STREAM_NO_CLOSE)
		return 0;
	return smap_stream_close(tp->transport);
}

static int
_trace_ioctl(struct _smap_stream *stream, int code, void *ptr)
{
	struct trace_stream *sp = (struct trace_stream *) stream;
	smap_transport_t (*optrans)[2], *iptrans;

	switch (code) {
	case SMAP_IOCTL_GET_TRANSPORT:
		if (!ptr)
			return EINVAL;
		optrans = ptr;
		(*optrans)[0] = (smap_transport_t) sp->transport;
		(*optrans)[1] = NULL;
		break;

	case SMAP_IOCTL_SET_TRANSPORT:
		if (!ptr)
			return EINVAL;
		iptrans = ptr;
		sp->transport = (smap_stream_t)iptrans[0];
		break;

	case SMAP_IOCTL_SET_ARGS:
		if (!ptr)
			return EINVAL;
		_trace_free_pfx(sp);
		_trace_new_pfx(sp, (char**)ptr);
		break;

	default:
		return EINVAL;
	}
	return 0;
}


static void
_trace_destroy(struct _smap_stream *stream)
{
	struct trace_stream *tp = (struct trace_stream *) stream;

	if (!(stream->flags & SMAP_STREAM_NO_CLOSE))
		smap_stream_destroy(&tp->transport);
	_trace_free_pfx(tp);
}

static int
_trace_flush(struct _smap_stream *stream)
{
	struct trace_stream *p = (struct trace_stream *)stream;
	if (!p->transport)
		return 0;
	return smap_stream_flush(p->transport);
}

int
smap_trace_stream_create(smap_stream_t *pstream, smap_stream_t transport)
{
	struct trace_stream *p;
	smap_stream_t stream;

	p = (struct trace_stream *)
		_smap_stream_create(sizeof(*p), SMAP_STREAM_WRITE);
	p->transport = transport;

	p->base.write = _trace_write;
	p->base.flush = _trace_flush;
	p->base.close = _trace_close;
	p->base.done = _trace_destroy;
	p->base.ctl  = _trace_ioctl;
	stream = (smap_stream_t)p;
	smap_stream_set_buffer(stream, smap_buffer_line, 1024);
	*pstream = stream;
	return 0;
}
