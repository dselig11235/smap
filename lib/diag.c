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
#include <stdio.h>
#include "smap/stream.h"
#include "smap/diag.h"

smap_stream_t smap_error_str;
smap_stream_t smap_debug_str;
smap_stream_t smap_trace_str;

void
smap_verror(const char *fmt, va_list ap)
{
	if (smap_stream_vprintf(smap_error_str, fmt, ap) < 0) {
		vfprintf(stderr, fmt, ap);
		fputc('\n', stderr);
		fprintf(stderr,
			"INTERNAL ERROR: smap_error_str not "
			"initialized, please report\n");
		abort();
	}
	smap_stream_write(smap_error_str, "\n", 1, NULL);
	smap_stream_flush(smap_error_str);
}

void
smap_error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	smap_verror(fmt, ap);
	va_end(ap);
}
