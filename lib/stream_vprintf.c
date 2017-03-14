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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <smap/stream.h>
#include <smap/streamdef.h>
#include <smap/printf.h>

int
smap_stream_vprintf(smap_stream_t str, const char *fmt, va_list ap)
{
	char *buf = NULL;
	size_t buflen = 0;
	size_t n;
	int rc;

	rc = smap_vasnprintf(&buf, &buflen, fmt, ap);
	if (rc)
		return rc;
	n = strlen(buf);
	rc = smap_stream_write (str, buf, n, NULL);
	free(buf);
	return rc == 0 ? n : -1;
}
