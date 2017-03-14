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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <smap/printf.h>

int
smap_asnprintf(char **pbuf, size_t *psize, const char *fmt, ...)
{
	int rc;
	va_list ap;

	va_start (ap, fmt);
	rc = smap_vasnprintf(pbuf, psize, fmt, ap);
	va_end (ap);
	return rc;
}
