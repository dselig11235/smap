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

#ifndef __SMAP_STREAM_H
#define __SMAP_STREAM_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

enum smap_buffer_type {
	smap_buffer_none,
	smap_buffer_line,
	smap_buffer_full
};

typedef struct _smap_transport *smap_transport_t;
typedef struct _smap_stream *smap_stream_t;

/* FIXME */
typedef off_t smap_off_t;

#define SMAP_SEEK_SET      0
#define SMAP_SEEK_CUR      1
#define SMAP_SEEK_END      2

#define SMAP_STREAM_READ	0x00000001
#define SMAP_STREAM_WRITE	0x00000002
#define SMAP_STREAM_RDWR        (SMAP_STREAM_READ|SMAP_STREAM_WRITE)
#define SMAP_STREAM_SEEK        0x00000004
#define SMAP_STREAM_APPEND      0x00000008
#define SMAP_STREAM_CREAT	0x00000010
#define SMAP_STREAM_NONBLOCK    0x00000020
#define SMAP_STREAM_NO_CLOSE    0x00000040
#define SMAP_STREAM_EXPBUF      0x00000080

#define SMAP_IOCTL_GET_TRANSPORT   1
#define SMAP_IOCTL_SET_TRANSPORT   2
#define SMAP_IOCTL_SET_DEBUG_IDX   3
#define SMAP_IOCTL_SET_DEBUG_PFX   4
#define SMAP_IOCTL_SET_ARGS        5

void smap_stream_ref(smap_stream_t stream);
void smap_stream_unref(smap_stream_t stream);
void smap_stream_destroy(smap_stream_t *pstream);
int smap_stream_open(smap_stream_t stream);
const char *smap_stream_strerror(smap_stream_t stream, int rc);
int smap_stream_err(smap_stream_t stream);
int smap_stream_last_error(smap_stream_t stream);
void smap_stream_clearerr(smap_stream_t stream);
int smap_stream_eof(smap_stream_t stream);
int smap_stream_seek(smap_stream_t stream, smap_off_t offset, int whence,
		     smap_off_t *pres);
int smap_stream_skip_input_bytes(smap_stream_t stream, smap_off_t count,
				 smap_off_t *pres);

int smap_stream_set_buffer(smap_stream_t stream, enum smap_buffer_type type,
			   size_t size);
int smap_stream_read(smap_stream_t stream, void *buf, size_t size,
		     size_t *pread);
int smap_stream_readdelim(smap_stream_t stream, char *buf, size_t size,
			  int delim, size_t *pread);
int smap_stream_readline(smap_stream_t stream, char *buf, size_t size,
			 size_t *pread);
int smap_stream_getdelim(smap_stream_t stream, char **pbuf, size_t *psize,
			 int delim, size_t *pread);
int smap_stream_getline(smap_stream_t stream, char **pbuf, size_t *psize,
			size_t *pread);
int smap_stream_write(smap_stream_t stream, const void *buf, size_t size,
		      size_t *pwrite);
int smap_stream_writeline(smap_stream_t stream, const char *buf, size_t size);
int smap_stream_flush(smap_stream_t stream);
int smap_stream_close(smap_stream_t stream);
int smap_stream_size(smap_stream_t stream, smap_off_t *psize);
smap_off_t smap_stream_bytes_in(smap_stream_t stream);
smap_off_t smap_stream_bytes_out(smap_stream_t stream);
int smap_stream_ioctl(smap_stream_t stream, int code, void *ptr);
int smap_stream_truncate(smap_stream_t stream, smap_off_t);
int smap_stream_shutdown(smap_stream_t stream, int how);

#define SMAP_STREAM_READY_RD 0x1
#define SMAP_STREAM_READY_WR 0x2
#define SMAP_STREAM_READY_EX 0x4
struct timeval;  /* Needed for the following declaration */

int smap_stream_wait(smap_stream_t stream, int *pflags, struct timeval *);

void smap_stream_get_flags(smap_stream_t stream, int *pflags);
int smap_stream_set_flags(smap_stream_t stream, int fl);
int smap_stream_clr_flags(smap_stream_t stream, int fl);

int smap_stream_vprintf(smap_stream_t str, const char *fmt, va_list ap);
int smap_stream_printf(smap_stream_t stream, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));


int smap_fileout_stream_create (smap_stream_t *pstream, FILE *file,
				const char *pfx, int pgopt);
int smap_syslog_stream_create (smap_stream_t *pstream, int prio,
			       const char *pfx);

int smap_sockmap_stream_create(smap_stream_t *pstream, int fd, int flags);
int smap_sockmap_stream_create2(smap_stream_t *pstream, int fd[], int flags);

int smap_transcript_stream_create(smap_stream_t *pstream,
				  smap_stream_t transport,
				  smap_stream_t logstr,
				  const char *prefix[]);
int smap_trace_stream_create(smap_stream_t *pstream, smap_stream_t transport);

#endif
