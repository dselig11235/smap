/* This file is part of Smap.
   Copyright (C) 2009-2010, 2014 Sergey Poznyakoff

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
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#ifndef SIZE_MAX
# define SIZE_MAX (~((size_t)0))
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <smap/stream.h>
#include <smap/streamdef.h>

static int
_stream_seterror(struct _smap_stream *stream, int code, int perm)
{
	stream->last_err = code;
	switch (code) {
	case 0:
	case EAGAIN:
	case EINPROGRESS:
		break;

	default:
		if (perm)
			stream->flags |= _SMAP_STR_ERR;
	}
	return code;
}

static int
_stream_realloc_buffer(struct _smap_stream *stream, size_t newsize)
{
	char *newbuf;
	size_t d;

	if (stream->bufsize == SIZE_MAX)
		return ENOMEM;

	if (!newsize) {
		newsize = 2 * stream->bufsize;
		if (newsize < stream->bufsize)
			newsize = SIZE_MAX;
	}
	newbuf = realloc(stream->buffer, newsize);
	if (!newbuf)
		return ENOMEM;
	d = stream->cur - stream->buffer;
	stream->buffer = newbuf;
	stream->bufsize = newsize;
	stream->cur = stream->buffer + d;
	return 0;
}

static int
_stream_fill_full_buffered(struct _smap_stream *stream)
{
	int rc;

	stream->level = 0;
	while (1) {
		size_t nrd = 0;

		stream->flags &= ~_SMAP_STR_MORESPC;
		rc = smap_stream_read_unbuffered(stream,
			 stream->buffer + stream->level,
			 stream->bufsize - stream->level,
			 0, &nrd);
		if (rc == ERANGE && (stream->flags & _SMAP_STR_MORESPC)) {
			smap_stream_clearerr(stream);
			rc = _stream_realloc_buffer(stream, nrd);
			if (rc)
				return rc;
			continue;
		}

		stream->level += nrd;
		break;
	}
	return rc;
}

static int
_stream_fill_line_buffered(struct _smap_stream *stream)
{
	int rc;
	size_t rdn;
	size_t n;
	char c;

	for (n = 0;
	     n < stream->bufsize &&
		     (rc = smap_stream_read_unbuffered(stream,
						       &c, 1,
						       0, &rdn)) == 0
		     && rdn; ) {
		stream->buffer[n++] = c;
		if (c == '\n')
			break;

		if (n == stream->bufsize
		    && stream->flags & SMAP_STREAM_EXPBUF) {
			rc = _stream_realloc_buffer(stream, 0);
			if (rc)
				break;
		}
	}
	stream->level = n;
	return rc;
}

static int
_stream_fill_buffer(struct _smap_stream *stream)
{
	int rc = 0;

	switch (stream->buftype) {
	case smap_buffer_none:
		return 0;

	case smap_buffer_full:
		rc = _stream_fill_full_buffered(stream);
		break;

	case smap_buffer_line:
		rc = _stream_fill_line_buffered(stream);
		break;
	}
	stream->cur = stream->buffer;
	return rc;
}

#define BUFFER_FULL_P(s) \
  ((s)->cur + (s)->level == (s)->buffer + (s)->bufsize)

static int
_stream_buffer_full_p(struct _smap_stream *stream)
{
	switch (stream->buftype) {
	case smap_buffer_none:
		break;

	case smap_buffer_line:
		return BUFFER_FULL_P(stream)
			|| memchr(stream->cur, '\n', stream->level) != NULL;

	case smap_buffer_full:
		return BUFFER_FULL_P(stream);
	}
	return 0;
}

static int
_force_flush_buffer(struct _smap_stream *stream)
{
	int rc = smap_stream_write_unbuffered(stream,
					      stream->cur,
					      stream->level,
					      1, NULL);
	if (rc == 0)
		_stream_advance_buffer(stream, stream->level);
	return rc;
}

static int
_stream_flush_buffer(struct _smap_stream *stream, int all)
{
	int rc;
	char *end;

	if (stream->flags & _SMAP_STR_DIRTY) {
		if ((stream->flags & SMAP_STREAM_SEEK)
		    && (rc = smap_stream_seek(stream, stream->offset,
					      SMAP_SEEK_SET, NULL)))
			return rc;

		switch (stream->buftype) {
		case smap_buffer_none:
			abort(); /* should not happen */

		case smap_buffer_full:
			if ((rc = smap_stream_write_unbuffered(stream,
							       stream->cur,
							       stream->level,
							       1, NULL)))
				return rc;
			if (all)
				_stream_advance_buffer(stream, stream->level);
			break;

		case smap_buffer_line:
			if (stream->level == 0)
				break;
			for (end = memchr(stream->cur, '\n', stream->level);
			     end;
			     end = memchr(stream->cur, '\n', stream->level)) {
				size_t size = end - stream->cur + 1;
				rc = smap_stream_write_unbuffered(stream,
								  stream->cur,
								  size, 1,
								  NULL);
				if (rc)
					return rc;
				_stream_advance_buffer(stream, size);
			}
			if (stream->level) {
				if (all) {
					if (rc = _force_flush_buffer(stream))
						return rc;
				} else if (BUFFER_FULL_P(stream)) {
					if (stream->flags & SMAP_STREAM_EXPBUF)
						return _stream_realloc_buffer(stream, 0);
					if (rc = _force_flush_buffer(stream))
						return rc;
				}
			}
		}
	} else if (all)
		_stream_advance_buffer(stream, stream->level);

	if (stream->level) {
		if (stream->cur > stream->buffer)
			memmove(stream->buffer, stream->cur, stream->level);
	} else {
		stream->flags &= ~_SMAP_STR_DIRTY;
		stream->level = 0;
	}
	stream->cur = stream->buffer;
	return 0;
}


smap_stream_t
_smap_stream_create(size_t size, int flags)
{
	struct _smap_stream *str;
	if (size < sizeof(str))
		abort();
	str = calloc(1, size);
	if (!str)
		return str;
	str->flags = flags;
	smap_stream_ref(str);
	return str;
}

void
smap_stream_destroy(smap_stream_t *pstream)
{
	if (pstream) {
		smap_stream_t str = *pstream;
		if (str && (str->ref_count == 0 || --str->ref_count == 0)) {
			smap_stream_close(str);
			if (str->done)
				str->done(str);
			free(str);
			*pstream = NULL;
		}
	}
}

void
smap_stream_get_flags(smap_stream_t str, int *pflags)
{
	*pflags = str->flags & ~_SMAP_STR_INTERN_MASK;
}

void
smap_stream_ref(smap_stream_t stream)
{
	stream->ref_count++;
}

void
smap_stream_unref(smap_stream_t stream)
{
	smap_stream_destroy(&stream);
}

int
smap_stream_open(smap_stream_t stream)
{
	int rc;

	if (stream->open && (rc = stream->open(stream)))
		return _stream_seterror(stream, rc, 1);
	stream->bytes_in = stream->bytes_out = 0;
	return 0;
}

const char *
smap_stream_strerror(smap_stream_t stream, int rc)
{
	const char *str;

	if (stream->error_string)
		str = stream->error_string(stream, rc);
	else
		str = strerror(rc);
	return str;
}

int
smap_stream_err(smap_stream_t stream)
{
	return stream->flags & _SMAP_STR_ERR;
}

int
smap_stream_last_error(smap_stream_t stream)
{
	return stream->last_err;
}

void
smap_stream_clearerr(smap_stream_t stream)
{
	stream->last_err = 0;
	stream->flags &= ~_SMAP_STR_ERR;
}

int
smap_stream_eof(smap_stream_t stream)
{
	return stream->flags & _SMAP_STR_EOF;
}

int
smap_stream_seek(smap_stream_t stream, smap_off_t offset, int whence,
		 smap_off_t *pres)
{
	int rc;
	smap_off_t size;

	if (!stream->seek)
		return _stream_seterror(stream, ENOSYS, 0);

	if (!(stream->flags & SMAP_STREAM_SEEK))
		return _stream_seterror(stream, EACCES, 1);

	switch (whence) {
	case SMAP_SEEK_SET:
		break;

	case SMAP_SEEK_CUR:
		if (offset == 0) {
			*pres = stream->offset + _stream_buffer_offset(stream);
			return 0;
		}
		offset += stream->offset;
		break;

	case SMAP_SEEK_END:
		rc = smap_stream_size(stream, &size);
		if (rc)
			return _stream_seterror(stream, rc, 1);
		offset += size;
		break;

	default:
		return _stream_seterror(stream, EINVAL, 1);
	}

	if (stream->buftype == smap_buffer_none
	    || offset < stream->offset
	    || offset > stream->offset + _stream_buffer_offset(stream)) {
		if ((rc = _stream_flush_buffer(stream, 1)))
			return rc;
		rc = stream->seek(stream, offset, &stream->offset);
		if (rc)
			return _stream_seterror(stream, rc, 1);
		_stream_cleareof(stream);
	}

	if (pres)
		*pres = stream->offset + _stream_buffer_offset(stream);
	return 0;
}

/* Skip COUNT bytes from the current position in stream by reading from
   it.  Return new offset in PRES.

   Return 0 on success, EACCES if STREAM was not opened for reading.
   Another non-zero exit codes are propagated from the underlying
   input operations.

   This function is designed to help implement seek method in otherwise
   unseekable streams (such as filters).  Do not use it if you absolutely
   have to.  Using it on an unbuffered stream is a terrible waste of CPU. */
int
smap_stream_skip_input_bytes(smap_stream_t stream, smap_off_t count,
			     smap_off_t *pres)
{
	smap_off_t pos;
	int rc;

	if (!(stream->flags & SMAP_STREAM_READ))
		return _stream_seterror(stream, EACCES, 1);

	if (stream->buftype == smap_buffer_none) {
		for (pos = 0; pos < count; pos++) {
			char c;
			size_t nrd;
			rc = smap_stream_read(stream, &c, 1, &nrd);
			if (nrd == 0)
				rc = ESPIPE;
			if (rc)
				break;
		}
	} else {
		for (pos = 0;;)	{
			if ((rc = _stream_flush_buffer(stream, 1)))
				return rc;
			if (stream->level == 0) {
				rc = _stream_fill_buffer(stream);
				if (rc)
					break;
				if (stream->level == 0) {
					rc = ESPIPE;
					break;
				}
			}
			if (pos <= count && count < pos + stream->level) {
				size_t delta = count - pos;
				_stream_advance_buffer(stream, delta);
				pos = count;
				rc = 0;
				break;
			}
			pos += stream->level;
		}
	}

	stream->offset += pos;
	if (pres)
		*pres = stream->offset;
	return rc;
}

int
smap_stream_set_buffer(smap_stream_t stream, enum smap_buffer_type type,
		       size_t size)
{
	if (size == 0)
		type = smap_buffer_none;

	if (stream->buffer) {
		smap_stream_flush(stream);
		free(stream->buffer);
	}

	stream->buftype = type;
	if (type == smap_buffer_none) {
		stream->buffer = NULL;
		return 0;
	}

	stream->buffer = malloc(size);
	if (stream->buffer == NULL) {
		stream->buftype = smap_buffer_none;
		return _stream_seterror(stream, ENOMEM, 1);
	}
	stream->bufsize = size;
	stream->cur = stream->buffer;
	stream->level = 0;

	return 0;
}

int
smap_stream_read_unbuffered(smap_stream_t stream, void *buf, size_t size,
			    int full_read,
			    size_t *pnread)
{
	int rc;
	size_t nread;

	if (!stream->read)
		return _stream_seterror(stream, ENOSYS, 0);

	if (!(stream->flags & SMAP_STREAM_READ))
		return _stream_seterror(stream, EACCES, 1);

	if (stream->flags & _SMAP_STR_ERR)
		return stream->last_err;

	if ((stream->flags & _SMAP_STR_EOF) || size == 0) {
		if (pnread)
			*pnread = 0;
		return 0;
	}

	if (full_read) {
		size_t rdbytes;

		nread = 0;
		while (size > 0
		       && (rc = stream->read(stream, buf, size, &rdbytes))
			    == 0) {
			if (rdbytes == 0) {
				stream->flags |= _SMAP_STR_EOF;
				break;
			}
			buf += rdbytes;
			nread += rdbytes;
			size -= rdbytes;
			stream->bytes_in += rdbytes;
		}
		if (size && rc)
			rc = _stream_seterror(stream, rc, 0);
	} else {
		rc = stream->read(stream, buf, size, &nread);
		if (rc == 0) {
			if (nread == 0)
				stream->flags |= _SMAP_STR_EOF;
			stream->bytes_in += nread;
		}
		_stream_seterror(stream, rc, rc != 0);
	}
	stream->offset += nread;
	if (pnread)
		*pnread = nread;

	return rc;
}

int
smap_stream_write_unbuffered(smap_stream_t stream,
			     const void *buf, size_t size,
			     int full_write,
			     size_t *pnwritten)
{
	int rc;
	size_t nwritten;

	if (!stream->write)
		return _stream_seterror(stream, ENOSYS, 0);

	if (!(stream->flags & SMAP_STREAM_WRITE))
		return _stream_seterror(stream, EACCES, 1);

	if (stream->flags & _SMAP_STR_ERR)
		return stream->last_err;

	if (size == 0) {
		if (pnwritten)
			*pnwritten = 0;
		return 0;
	}

	if (full_write) {
		size_t wrbytes;
		const char *bufp = buf;

		nwritten = 0;
		while (size > 0
		       && (rc = stream->write(stream, bufp, size, &wrbytes))
			    == 0) {
			if (wrbytes == 0) {
				rc = EIO;
				break;
			}
			bufp += wrbytes;
			nwritten += wrbytes;
			size -= wrbytes;
			stream->bytes_out += wrbytes;
		}
	} else {
		rc = stream->write(stream, buf, size, &nwritten);
		if (rc == 0)
			stream->bytes_out += nwritten;
	}
	stream->flags |= _SMAP_STR_WRT;
	stream->offset += nwritten;
	if (pnwritten)
		*pnwritten = nwritten;
	_stream_seterror(stream, rc, rc != 0);
	return rc;
}

int
smap_stream_read(smap_stream_t stream, void *buf, size_t size, size_t *pread)
{
	if (stream->buftype == smap_buffer_none)
		return smap_stream_read_unbuffered(stream, buf, size,
						   !pread, pread);
	else {
		char *bufp = buf;
		size_t nbytes = 0;
		while (size) {
			size_t n;
			int rc;

			if (stream->level == 0) {
				if ((rc = _stream_fill_buffer(stream))) {
					if (nbytes)
						break;
					return rc;
				}
				if (stream->level == 0)
					break;
			}

			n = size;
			if (n > stream->level)
				n = stream->level;
			memcpy(bufp, stream->cur, n);
			_stream_advance_buffer(stream, n);
			nbytes += n;
			bufp += n;
			size -= n;
			if (stream->buftype == smap_buffer_line
			    && bufp[-1] == '\n')
				break;
		}

		if (pread)
			*pread = nbytes;
	}
	return 0;
}

int
_stream_scandelim(smap_stream_t stream, char *buf, size_t size, int delim,
		  size_t *pnread)
{
	int rc = 0;
	size_t nread = 0;

	size--;
	if (size == 0)
		return EOVERFLOW;
	while (size) {
		char *p;
		size_t len;

		if (stream->level == 0) {
			if ((rc = _stream_fill_buffer(stream))
			    || stream->level == 0)
				break;
		}

		p = memchr(stream->cur, delim, stream->level);
		len = p ? p - stream->cur + 1 : stream->level;
		if (len > size)
			len = size;
		memcpy(buf, stream->cur, len);
		_stream_advance_buffer(stream, len);
		buf += len;
		size -= len;
		nread += len;
		if (p) /* Delimiter found */
			break;
	}
	*buf = 0;
	*pnread = nread;
	return rc;
}

static int
_stream_readdelim(smap_stream_t stream, char *buf, size_t size,
		  int delim, size_t *pread)
{
	int rc;
	char c;
	size_t n = 0, rdn;

	size--;
	if (size == 0)
		return EOVERFLOW;
	for (n = 0;
	     n < size &&
		     (rc = smap_stream_read(stream, &c, 1, &rdn)) == 0 &&
		     rdn;) {
		*buf++ = c;
		n++;
		if (c == delim)
			break;
	}
	*buf = 0;
	if (pread)
		*pread = n;
	return rc;
}

int
smap_stream_readdelim(smap_stream_t stream, char *buf, size_t size,
		      int delim, size_t *pread)
{
	int rc;

	if (size == 0)
		return EINVAL;

	if (stream->readdelim)
		rc = stream->readdelim(stream, buf, size, delim, pread);
	else if (stream->buftype != smap_buffer_none)
		rc = _stream_scandelim(stream, buf, size, delim, pread);
	else
		rc = _stream_readdelim(stream, buf, size, delim, pread);
	return rc;
}

int
smap_stream_readline(smap_stream_t stream, char *buf, size_t size,
		     size_t *pread)
{
	return smap_stream_readdelim(stream, buf, size, '\n', pread);
}

int
smap_stream_getdelim(smap_stream_t stream, char **pbuf, size_t *psize,
		     int delim, size_t *pread)
{
	int rc;
	char *lineptr = *pbuf;
	size_t n = *psize;
	size_t cur_len = 0;

	if (lineptr == NULL || n == 0) {
		char *new_lineptr;
		n = 120;
		new_lineptr = realloc(lineptr, n);
		if (new_lineptr == NULL)
			return ENOMEM;
		lineptr = new_lineptr;
	}

	for (;;) {
		size_t rdn;

		/* Make enough space for len+1 (for final NUL) bytes.  */
		if (cur_len + 1 >= n) {
			size_t needed_max =
				SSIZE_MAX < SIZE_MAX ? (size_t) SSIZE_MAX + 1
						     : SIZE_MAX;
			size_t needed = 2 * n + 1;   /* Be generous. */
			char *new_lineptr;

			if (needed_max < needed)
				needed = needed_max;
			if (cur_len + 1 >= needed) {
				rc = EOVERFLOW;
				break;
			}

			new_lineptr = realloc(lineptr, needed);
			if (new_lineptr == NULL) {
				rc = ENOMEM;
				break;
			}

			lineptr = new_lineptr;
			n = needed;
		}

		if (stream->readdelim)
			rc = stream->readdelim(stream, lineptr + cur_len,
					       n - cur_len, delim, &rdn);
		else if (stream->buftype != smap_buffer_none)
			rc = _stream_scandelim(stream, lineptr + cur_len,
					       n - cur_len, delim, &rdn);
		else
			rc = smap_stream_read(stream, lineptr + cur_len, 1,
					      &rdn);

		if (rc || rdn == 0)
			break;
		cur_len += rdn;

		if (lineptr[cur_len - 1] == delim)
			break;
	}
	lineptr[cur_len] = '\0';

	*pbuf = lineptr;
	*psize = n;

	if (pread)
		*pread = cur_len;
	return rc;
}

int
smap_stream_getline(smap_stream_t stream, char **pbuf, size_t *psize,
		    size_t *pread)
{
	return smap_stream_getdelim(stream, pbuf, psize, '\n', pread);
}

int
smap_stream_write(smap_stream_t stream, const void *buf, size_t size,
		  size_t *pnwritten)
{
	int rc = 0;

	if (stream->buftype == smap_buffer_none)
		rc = smap_stream_write_unbuffered(stream, buf, size,
						  !pnwritten, pnwritten);
	else {
		size_t nbytes = 0;
		const char *bufp = buf;

		while (1) {
			size_t n;

			if (_stream_buffer_full_p(stream)
			    && (rc = _stream_flush_buffer(stream, 0)))
				break;

			if (size == 0)
				break;

			n = stream->bufsize - stream->level;
			if (n > size)
				n = size;
			memcpy(stream->cur + stream->level, bufp, n);
			stream->level += n;
			nbytes += n;
			bufp += n;
			size -= n;
			stream->flags |= _SMAP_STR_DIRTY;
		}
		if (pnwritten)
			*pnwritten = nbytes;
	}
	return rc;
}

int
smap_stream_writeline(smap_stream_t stream, const char *buf, size_t size)
{
	int rc;
	if ((rc = smap_stream_write(stream, buf, size, NULL)) == 0)
		rc = smap_stream_write(stream, "\r\n", 2, NULL);
	return rc;
}

int
smap_stream_flush(smap_stream_t stream)
{
	int rc;

	if (!stream)
		return EINVAL;
	rc = _stream_flush_buffer(stream, 1);
	if (rc)
		return rc;
	if ((stream->flags & _SMAP_STR_WRT) && stream->flush)
		return stream->flush(stream);
	stream->flags &= ~_SMAP_STR_WRT;
	return 0;
}

int
smap_stream_close(smap_stream_t stream)
{
	int rc = 0;

	if (!stream)
		return EINVAL;
	smap_stream_flush(stream);
	/* Do close the stream only if it is not used by anyone else */
	if (stream->ref_count > 1)
		return 0;
	if (stream->close)
		rc = stream->close(stream);
	return rc;
}

int
smap_stream_size(smap_stream_t stream, smap_off_t *psize)
{
	int rc;

	if (!stream->size)
		return _stream_seterror(stream, ENOSYS, 0);
	rc = stream->size(stream, psize);
	return _stream_seterror(stream, rc, rc != 0);
}

smap_off_t
smap_stream_bytes_in(smap_stream_t stream)
{
	return stream->bytes_in;
}

smap_off_t
smap_stream_bytes_out(smap_stream_t stream)
{
	return stream->bytes_out;
}

int
smap_stream_ioctl(smap_stream_t stream, int code, void *ptr)
{
	if (stream->ctl == NULL)
		return ENOSYS;
	return stream->ctl(stream, code, ptr);
}

int
smap_stream_wait(smap_stream_t stream, int *pflags, struct timeval *tvp)
{
	int flg = 0;
	if (stream == NULL)
		return EINVAL;

	/* Take to account if we have any buffering.  */
	/* FIXME: How about SMAP_STREAM_READY_WR? */
	if ((*pflags) & SMAP_STREAM_READY_RD
	    && stream->buftype != smap_buffer_none
	    && stream->level > 0) {
		flg = SMAP_STREAM_READY_RD;
		*pflags &= ~SMAP_STREAM_READY_RD;
	}

	if (stream->wait) {
		int rc = stream->wait(stream, pflags, tvp);
		if (rc == 0)
			*pflags |= flg;
		return rc;
	}

	return ENOSYS;
}

int
smap_stream_truncate(smap_stream_t stream, smap_off_t size)
{
	if (stream->truncate)
		return stream->truncate(stream, size);
	return ENOSYS;
}

int
smap_stream_shutdown(smap_stream_t stream, int how)
{
	if (stream->shutdown)
		return stream->shutdown(stream, how);
	return ENOSYS;
}

int
smap_stream_set_flags(smap_stream_t stream, int fl)
{
	if (stream == NULL)
		return EINVAL;
	stream->flags |= (fl & ~_SMAP_STR_INTERN_MASK);
	return 0;
}

int
smap_stream_clr_flags(smap_stream_t stream, int fl)
{
	if (stream == NULL)
		return EINVAL;
	stream->flags &= ~(fl & ~_SMAP_STR_INTERN_MASK);
	return 0;
}
