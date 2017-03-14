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

#ifndef __SMAP_STREAMDEF_H
#define __SMAP_STREAMDEF_H

#define _SMAP_STR_DIRTY         0x01000    /* Buffer dirty */
#define _SMAP_STR_WRT           0x02000    /* Unflushed write pending */
#define _SMAP_STR_ERR           0x04000    /* Permanent error state */
#define _SMAP_STR_EOF           0x08000    /* EOF encountered */
#define _SMAP_STR_MORESPC       0x10000
#define _SMAP_STR_INTERN_MASK   0xff000

struct _smap_stream {
	int ref_count;

	enum smap_buffer_type buftype;
	size_t bufsize;
	char *buffer;
	size_t level;
	char *cur;

	int flags;
	smap_off_t offset;
	smap_off_t bytes_in, bytes_out;

	int last_err;

	int (*read)(struct _smap_stream *, char *, size_t, size_t *);
	int (*readdelim)(struct _smap_stream *, char *, size_t, int, size_t *);
	int (*write)(struct _smap_stream *, const char *, size_t, size_t *);
	int (*flush)(struct _smap_stream *);
	int (*open)(struct _smap_stream *);
	int (*close)(struct _smap_stream *);
	void (*done)(struct _smap_stream *);
	int (*seek)(struct _smap_stream *, smap_off_t, smap_off_t *);
	int (*size)(struct _smap_stream *, smap_off_t *);
	int (*ctl)(struct _smap_stream *, int, void *);
	int (*wait)(struct _smap_stream *, int *, struct timeval *);
	int (*truncate)(struct _smap_stream *, smap_off_t);
	int (*shutdown)(struct _smap_stream *, int);

	const char *(*error_string)(struct _smap_stream *, int);

};

#define _stream_cleareof(s) ((s)->flags &= ~_SMAP_STR_EOF)
#define _stream_advance_buffer(s,n) ((s)->cur += n, (s)->level -= n)
#define _stream_buffer_offset(s) ((s)->cur - (s)->buffer)
#define _stream_orig_level(s) ((s)->level + _stream_buffer_offset(s))

smap_stream_t _smap_stream_create(size_t size, int flags);
int smap_stream_read_unbuffered(smap_stream_t stream, void *buf, size_t size,
				int full_read, size_t *pnread);
int smap_stream_write_unbuffered(smap_stream_t stream,
				 const void *buf, size_t size,
				 int full_write, size_t *pnwritten);

#endif
