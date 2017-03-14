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

/* A "transcript stream" transparently writes data to and reads data from
   an underlying transport stream, writing each lineful of data to a "log
   stream". Writes to log stream are prefixed with a string indicating
   direction of the data (read/write). Default prefixes are those used in
   most RFCs - "S: " for data written ("Server"), and "C: " for data read
   ("Client"). */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <smap/stream.h>
#include <smap/streamdef.h>

#define TRANS_READ 0x1
#define TRANS_WRITE 0x2
#define FLAG_TO_PFX(c) ((c) - 1)

struct transcript_stream {
	struct _smap_stream base;
	int flags;
	smap_stream_t transport;
	smap_stream_t logstr;
	char *prefix[2];
};

static void
print_transcript(struct transcript_stream *str, int flag,
		 const char *buf, size_t size)
{
	while (size) {
		const char *p;
		size_t len;

		if (str->flags & flag) {
			smap_stream_write(str->logstr,
					  str->prefix[FLAG_TO_PFX(flag)],
					  strlen(str->prefix[FLAG_TO_PFX(flag)]),
				NULL);
			str->flags &= ~flag;
		}
		p = memchr(buf, '\n', size);
		if (p) {
			len = p - buf;
			if (p > buf && p[-1] == '\r')
				len--;
			smap_stream_write(str->logstr, buf, len, NULL);
			smap_stream_write(str->logstr, "\n", 1, NULL);
			str->flags |= flag;

			len = p - buf + 1;
			buf = p + 1;
			size -= len;
		} else {
			smap_stream_write(str->logstr, buf, size, NULL);
			break;
		}
	}
}

static int
transcript_read(struct _smap_stream *stream, char *buf, size_t size,
		size_t *pret)
{
	struct transcript_stream *p = (struct transcript_stream *)stream;
	size_t nbytes;

	if (smap_stream_read(p->transport, buf, size, &nbytes) == 0) {
		print_transcript(p, TRANS_READ, buf, nbytes);
		if (pret)
			*pret = nbytes;
	} else
		return smap_stream_last_error(p->transport);
	return 0;
}

static int
transcript_write(struct _smap_stream *stream, const char *buf,
		 size_t size, size_t *pret)
{
	struct transcript_stream *p = (struct transcript_stream *)stream;
	size_t n;
	if (smap_stream_write(p->transport, buf, size, &n) == 0) {
		print_transcript(p, TRANS_WRITE, buf, size);
		*pret = n;
	} else
		return smap_stream_last_error(p->transport);
	return 0;
}

static int
transcript_flush(struct _smap_stream *stream)
{
	struct transcript_stream *p = (struct transcript_stream *)stream;
	if (!p->transport)
		return 0;
	return smap_stream_flush(p->transport);
}

static int
transcript_close(struct _smap_stream *stream)
{
	struct transcript_stream *p = (struct transcript_stream *)stream;
	smap_stream_close(p->logstr);
	if (!(stream->flags & SMAP_STREAM_NO_CLOSE))
		smap_stream_close(p->transport);
	return 0;
}


static void
transcript_destroy(struct _smap_stream *stream)
{
	struct transcript_stream *p = (struct transcript_stream *)stream;

	free(p->prefix[0]);
	free(p->prefix[1]);
	smap_stream_destroy(&p->logstr);
	if (!(stream->flags & SMAP_STREAM_NO_CLOSE))
		smap_stream_destroy(&p->transport);
}

static const char *
transcript_strerror(struct _smap_stream *stream, int rc)
{
	struct transcript_stream *p = (struct transcript_stream *)stream;
	return smap_stream_strerror(p->transport, rc);
}

static int
transcript_ioctl(struct _smap_stream *stream, int code, void *ptr)
{
	struct transcript_stream *p = (struct transcript_stream *)stream;
	smap_transport_t (*optrans)[2], *iptrans;

	switch (code) {
	case SMAP_IOCTL_GET_TRANSPORT:
		if (!ptr)
			return EINVAL;
		optrans = ptr;
		(*optrans)[0] = (smap_transport_t) p->transport;
		(*optrans)[1] = NULL;
		break;

	case SMAP_IOCTL_SET_TRANSPORT:
		if (!ptr)
			return EINVAL;
		iptrans = ptr;
		p->transport = (smap_stream_t)iptrans[0];
		break;

	default:
		return EINVAL;
	}
	return 0;
}

const char *default_prefix[2] = {
    "C: ", "S: "
};

int
smap_transcript_stream_create(smap_stream_t *pstream,
			      smap_stream_t transport, smap_stream_t logstr,
			      const char *prefix[])
{
	struct transcript_stream *p;
	smap_stream_t stream;

	p = (struct transcript_stream *)
		 _smap_stream_create(sizeof(*p),
				     SMAP_STREAM_READ|SMAP_STREAM_WRITE);
	/* FIXME: Better perhaps:
	    transport->flags & ~_SMAP_STR_INTERN_MASK);
	   but that requires implementing all methods. */
	if (!p)
		return ENOMEM;

	p->flags = TRANS_READ | TRANS_WRITE;
	if (prefix) {
		p->prefix[0] = strdup(prefix[0] ? prefix[0]
						: default_prefix[0]);
		p->prefix[1] = strdup(prefix[1] ? prefix[1]
						: default_prefix[1]);
	} else {
		p->prefix[0] = strdup(default_prefix[0]);
		p->prefix[1] = strdup(default_prefix[1]);
	}
	p->transport = transport;
	p->logstr = logstr;

	p->base.read = transcript_read;
	p->base.write = transcript_write;
	p->base.flush = transcript_flush;
	p->base.close = transcript_close;
	p->base.done = transcript_destroy;
	p->base.ctl  = transcript_ioctl;
	p->base.error_string = transcript_strerror;
	stream = (smap_stream_t)p;
	smap_stream_set_buffer(stream, smap_buffer_line, 1024);
	*pstream = stream;
	return 0;
}
