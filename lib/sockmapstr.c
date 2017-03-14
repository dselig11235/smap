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

#include <sys/types.h>
#include <sys/socket.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "smap/diag.h"
#include "smap/stream.h"
#include "smap/streamdef.h"

/* Bound on length of the string representing a size_t.
   log10 (2.0) < 146/485; */
#define SIZE_T_STRLEN_BOUND ((sizeof(size_t) * CHAR_BIT) * 146 / 485 + 1)


struct sockmap_output_stream {
	struct _smap_stream base;
	int fd;
	size_t bufsize;
	char *buf;
	int debug_idx;
	char *debug_pfx;
};

static char *
format_len(char *buf, size_t arg)
{
	char *p = buf + SIZE_T_STRLEN_BOUND;

	*--p = 0;
	while (arg) {
		unsigned n = arg % 10;
		*--p = '0' + n;
		arg /= 10;
	}
	return p;
}

static int
_sockmap_output_stream_write(struct _smap_stream *stream, const char *buf,
			     size_t len, size_t *pret)
{
	struct sockmap_output_stream *sp =
		(struct sockmap_output_stream *)stream;
	char nbuf[SIZE_T_STRLEN_BOUND+1], *p;
	size_t size, n;

	if (smap_trace_str) {
		smap_stream_write(smap_trace_str, buf, len, NULL);
	}
	len--;
	p = format_len(nbuf, len);
	n = strlen(p);
	size = n + 3 + len;
	if (size > sp->bufsize) {
		char *newbuf = realloc(sp->buf, size);
		if (!newbuf)
			return ENOMEM;
		sp->buf = newbuf;
		sp->bufsize = size;
	}
	memcpy(sp->buf, p, n);
	sp->buf[n++] = ':';
	memcpy(sp->buf + n, buf, len);
	n += len;
	sp->buf[n++] = ',';
	sp->buf[n] = 0;

	if (smap_debug_np(sp->debug_idx, 10))
		smap_stream_printf(smap_debug_str, "%s: %s\n",
				   sp->debug_pfx ? sp->debug_pfx : "send",
				   sp->buf);

	if (write(sp->fd, sp->buf, n) != n)
		return errno;
	*pret = len + 1;
	return 0;
}

static int
_sockmap_output_stream_close(struct _smap_stream *stream)
{
	struct sockmap_output_stream *sp =
		(struct sockmap_output_stream *) stream;
	if (stream->flags & SMAP_STREAM_NO_CLOSE)
		return 0;
	if (close(sp->fd))
		return errno;
	return 0;
}

static int
_sockmap_output_stream_ioctl(struct _smap_stream *stream, int code, void *ptr)
{
	struct sockmap_output_stream *sp =
		(struct sockmap_output_stream *) stream;

	switch (code) {
	case SMAP_IOCTL_SET_DEBUG_IDX:
		if (!ptr)
			return EINVAL;
		sp->debug_idx = *(int*)ptr;
		break;

	case SMAP_IOCTL_SET_DEBUG_PFX:
		if (!ptr)
			return EINVAL;
		sp->debug_pfx = strdup((char*)ptr);
		break;

	default:
		return EINVAL;
	}
	return 0;
}

static void
_sockmap_output_stream_destroy(struct _smap_stream *stream)
{
	struct sockmap_output_stream *sp =
		(struct sockmap_output_stream *)stream;
	free(sp->buf);
	free(sp->debug_pfx);
}

int
smap_sockmap_output_stream_create(smap_stream_t *pstream, int fd, int flags)
{
	struct sockmap_output_stream *str =
		(struct sockmap_output_stream *)
		  _smap_stream_create(sizeof(*str),
				      SMAP_STREAM_WRITE |
					(flags & SMAP_STREAM_NO_CLOSE));
	if (!str)
		return ENOMEM;
	str->fd = fd;

	str->base.write = _sockmap_output_stream_write;
	str->base.close = _sockmap_output_stream_close;
	str->base.ctl = _sockmap_output_stream_ioctl;
	str->base.done = _sockmap_output_stream_destroy;
	*pstream = (smap_stream_t) str;
	return 0;
}

struct sockmap_input_stream {
	struct _smap_stream base;
	char nbuf[SIZE_T_STRLEN_BOUND+1]; /* Buffer for reading initial
					     segment of data */
	size_t nlen;                      /* Length of data stored in nbuf */
	size_t cp;                        /* Offset of colon in nbuf */
	size_t reqsz;                     /* Payload size */
	int fd;
	int debug_idx;
	char *debug_pfx;
};

static void
report_invalid_prefix(struct sockmap_input_stream *sp, const char *diag)
{
	static char buf[1024];
	size_t n;
	struct sockaddr_in saddr;
	socklen_t slen;

	slen = sizeof(saddr);
	if (getpeername(sp->fd, (struct sockaddr*) &saddr, &slen) != -1) {
		if (saddr.sin_family == AF_INET)
			smap_stream_printf(smap_debug_str,
					   "%s:%d: ",
					   inet_ntoa(saddr.sin_addr),
					   ntohs(saddr.sin_port));
		else if (saddr.sin_family == AF_UNIX)
			smap_stream_printf(smap_debug_str,
					   "[local socket]: ");
	}
	smap_stream_printf(smap_debug_str,
			   "sockmap protocol error "
			   "(%s): %s", diag, sp->nbuf);
	n = recv(sp->fd, buf, sizeof(buf), 0);
	if (n > 0)
		smap_stream_write(smap_debug_str, buf, n, NULL);
	smap_stream_write(smap_debug_str, "\n", 1, NULL);
}

int
read_payload_length(struct sockmap_input_stream *sp)
{
	int len = 0;

	sp->cp = 0;
	while (len < SIZE_T_STRLEN_BOUND) {
		ssize_t n;
		char *p;

		n = recv(sp->fd, sp->nbuf + len, SIZE_T_STRLEN_BOUND - len, 0);
		if (n < 0) {
			smap_debug(sp->debug_idx, 1,
				   ("error reading from fd #%d: %s", sp->fd,
				    strerror(errno)));
			return errno;
		}
		if (n == 0)
			return EOF;
		len += n;
		if (p = memchr(sp->nbuf, ':', len)) {
			sp->cp = p - sp->nbuf;
			sp->nbuf[len] = 0;
			sp->nlen = len;
			return 0;
		}
	}
	sp->nbuf[len] = 0;
	sp->nlen = len;
	if (smap_debug_np(sp->debug_idx, 1))
		report_invalid_prefix(sp, "prefix too long");
	return EPROTO;
}

#define ISDIGIT(c) ('0' <= (c) && (c) <= '9')

static int
_sockmap_input_stream_read(struct _smap_stream *stream, char *buf,
			    size_t size, size_t *pret)
{
	struct sockmap_input_stream *sp =
		(struct sockmap_input_stream *)stream;
	int rc;
	size_t reqsz;
	char *pb = buf;

	if (sp->nlen == 0) {
		size_t i;

		rc = read_payload_length(sp);
		if (rc == EOF) {
			*pret = 0;
			return 0;
		}
		if (smap_debug_np(sp->debug_idx, 10))
			smap_stream_printf(smap_debug_str, "%s: %s",
					   sp->debug_pfx
					      ? sp->debug_pfx : "recv",
					   sp->nbuf);
		if (rc)
			return rc;

		reqsz = 0;
		for (i = 0; i < sp->cp; i++) {
			reqsz *= 10;
			if (!ISDIGIT(sp->nbuf[i])) {
				if (smap_debug_np(sp->debug_idx, 1))
					report_invalid_prefix(sp,
							     "invalid prefix");
				sp->nlen = sp->cp = sp->reqsz = 0;
				return EPROTO;
			}
			reqsz += sp->nbuf[i] - '0';
		}
		sp->reqsz = reqsz;
	} else
		reqsz = sp->reqsz;

	reqsz++;
	if (reqsz > size) {
		*pret = reqsz;
		stream->flags |= _SMAP_STR_MORESPC;
		return ERANGE;
	}

	if (sp->nlen > sp->cp) {
		size_t rds = sp->nlen - sp->cp - 1;
		memcpy(buf, sp->nbuf + sp->cp + 1, rds);
		buf += rds;
		size -= rds;
		reqsz -= rds;
	}

	while (reqsz > 0) {
		size_t n = recv(sp->fd, buf, reqsz, 0);
		if (smap_debug_np(sp->debug_idx, 10))
			smap_stream_write(smap_debug_str, buf, n, NULL);
		if (n < 0)
			return errno;
		if (n == 0)
			return EIO;
		buf += n;
		size -= n;
		reqsz -= n;
	}

	if (smap_debug_np(sp->debug_idx, 10)) {
		smap_stream_write(smap_debug_str, "\n", 1, NULL);
		smap_stream_flush(smap_debug_str);
	}
	if (smap_trace_str) {
		smap_stream_write(smap_trace_str, pb, sp->reqsz, NULL);
		smap_stream_printf(smap_trace_str, " => ");
	}

	*pret = sp->reqsz + 1;
	sp->nlen = sp->cp = sp->reqsz = 0;

	if (buf[-1] == ',')
		buf[-1] = '\n';
	else {
		smap_debug(sp->debug_idx, 1, ("sockmap protocol error "
					      "(mising terminating comma)"));
		return EPROTO;
	}
	return 0;
}

static int
_sockmap_input_stream_close(struct _smap_stream *stream)
{
	struct sockmap_input_stream *sp =
		(struct sockmap_input_stream *) stream;
	if (stream->flags & SMAP_STREAM_NO_CLOSE)
		return 0;
	if (close(sp->fd))
		return errno;
	return 0;
}

static void
_sockmap_input_stream_destroy(struct _smap_stream *stream)
{
	struct sockmap_input_stream *sp =
		(struct sockmap_input_stream *)stream;
	free(sp->debug_pfx);
}

static int
_sockmap_input_stream_ioctl(struct _smap_stream *stream, int code, void *ptr)
{
	struct sockmap_input_stream *sp =
		(struct sockmap_input_stream *) stream;

	switch (code) {
	case SMAP_IOCTL_SET_DEBUG_IDX:
		if (!ptr)
			return EINVAL;
		sp->debug_idx = *(int*)ptr;
		break;

	case SMAP_IOCTL_SET_DEBUG_PFX:
		if (!ptr)
			return EINVAL;
		sp->debug_pfx = strdup((char*)ptr);
		break;

	default:
		return EINVAL;
	}
	return 0;
}

int
smap_sockmap_input_stream_create(smap_stream_t *pstream, int fd, int flags)
{
	struct sockmap_input_stream *str =
		(struct sockmap_input_stream *)
		  _smap_stream_create(sizeof(*str),
				      SMAP_STREAM_READ |
					(flags & SMAP_STREAM_NO_CLOSE));
	if (!str)
		return ENOMEM;
	str->fd = fd;

	str->base.read = _sockmap_input_stream_read;
	/* FIXME: Implement readdelim */
	str->base.close = _sockmap_input_stream_close;
	str->base.ctl = _sockmap_input_stream_ioctl;
	str->base.done = _sockmap_input_stream_destroy;
	*pstream = (smap_stream_t) str;
	return 0;
}

struct sockmap_stream {
	struct _smap_stream base;
	int fd;
	smap_stream_t in;
	smap_stream_t out;
};

static int
_sockmap_stream_write(struct _smap_stream *stream, const char *buf,
		      size_t size, size_t *pret)
{
	struct sockmap_stream *sp = (struct sockmap_stream *) stream;
	return smap_stream_write(sp->out, buf, size, pret);
}

static int
_sockmap_stream_read(struct _smap_stream *stream, char *buf,
		     size_t size, size_t *pret)
{
	struct sockmap_stream *sp = (struct sockmap_stream *) stream;
	return smap_stream_read(sp->in, buf, size, pret);
}

static int
_sockmap_stream_close(struct _smap_stream *stream)
{
	struct sockmap_stream *sp = (struct sockmap_stream *) stream;
	if (stream->flags & SMAP_STREAM_NO_CLOSE)
		return 0;
	smap_stream_close(sp->in);
	smap_stream_close(sp->out);
	if (sp->fd != -1 && close(sp->fd))
		return errno;
	return 0;
}

static void
_sockmap_stream_done(struct _smap_stream *stream)
{
	struct sockmap_stream *sp = (struct sockmap_stream *) stream;
	if (stream->flags & SMAP_STREAM_NO_CLOSE)
		return;
	smap_stream_destroy(&sp->in);
	smap_stream_destroy(&sp->out);
}

static int
_sockmap_stream_ioctl(struct _smap_stream *stream, int code, void *ptr)
{
	struct sockmap_stream *sp = (struct sockmap_stream *) stream;
	char **pfx;

	switch (code) {
	case SMAP_IOCTL_SET_DEBUG_IDX:
		if (!ptr)
			return EINVAL;
		smap_stream_ioctl(sp->in, code, ptr);
		smap_stream_ioctl(sp->out, code, ptr);
		break;

	case SMAP_IOCTL_SET_DEBUG_PFX:
		if (!ptr)
			return EINVAL;
		pfx = ptr;
		smap_stream_ioctl(sp->in, code, pfx[0]);
		smap_stream_ioctl(sp->out, code, pfx[1]);
		break;

	default:
		return EINVAL;
	}
	return 0;
}

int
smap_sockmap_stream_create2(smap_stream_t *pstream, int fd[], int flags)
{
	int rc;
	int sflags = 0;
	struct sockmap_stream *str =
		(struct sockmap_stream *)
		  _smap_stream_create(sizeof(*str),
				      SMAP_STREAM_READ|SMAP_STREAM_WRITE|
				      flags);
	if (!str)
		return ENOMEM;
	if (fd[0] == fd[1])
		sflags = SMAP_STREAM_NO_CLOSE;
	rc = smap_sockmap_output_stream_create(&str->out, fd[1], sflags);
	if (rc) {
		free(str);
		return rc;
	}

	rc = smap_sockmap_input_stream_create(&str->in, fd[0], sflags);
	if (rc) {
		free(str);
		smap_stream_destroy(&str->in);
		return rc;
	}

	smap_stream_set_buffer(str->out, smap_buffer_line, 1024);
	smap_stream_set_flags(str->out, SMAP_STREAM_EXPBUF);
	smap_stream_set_buffer(str->in, smap_buffer_full, 1024);
	smap_stream_set_flags(str->in, SMAP_STREAM_EXPBUF);

	str->base.read = _sockmap_stream_read;
	str->base.write = _sockmap_stream_write;
	str->base.close = _sockmap_stream_close;
	str->base.ctl = _sockmap_stream_ioctl;
	str->base.done = _sockmap_stream_done;
	str->fd = fd[0] == fd[1] ? fd[0] : -1;

	*pstream = (smap_stream_t)str;
	return 0;
}

int
smap_sockmap_stream_create(smap_stream_t *pstream, int fd, int flags)
{
	int pfd[2] = { fd, fd };
	return smap_sockmap_stream_create2(pstream, pfd, flags);
}
