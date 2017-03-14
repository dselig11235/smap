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

#ifndef __SMAP_DIAG_H
#define __SMAP_DIAG_H

#include "smap/stream.h"

extern smap_stream_t smap_error_str;
extern smap_stream_t smap_debug_str;
extern smap_stream_t smap_trace_str;

void smap_verror(const char *fmt, va_list ap);
void smap_error(const char *fmt, ...) __attribute__ ((__format__ (__printf__, 1, 2)));

extern char *smap_progname;

void smap_set_program_name(char *name);

void smap_openlog_stderr(int trace);
void smap_openlog_syslog(char *tag, int opts, int facility, int trace);

int smap_debug_np(size_t mod, size_t lev);
int smap_debug_sp(const char *name, size_t lev);
size_t smtp_debug_locate_n(const char *name, size_t len);
size_t smtp_debug_locate(const char *name);
size_t smap_debug_alloc_n(const char *name, size_t n);
size_t smap_debug_alloc(const char *name);
void smap_debug_printf(const char *fmt, ...)
	          __attribute__ ((__format__ (__printf__, 1, 2)));
int smap_debug_set(const char *spec);

#define smap_debug(num, lev, args)		\
	do {					\
		if (smap_debug_np(num, lev))	\
			smap_debug_printf args;	\
	} while (0)

#endif
