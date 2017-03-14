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

#ifndef __SMAP_IO_H
#define __SMAP_IO_H

#include <stdarg.h>

int smap_vasnprintf(char **pbuf, size_t *psize, const char *fmt, va_list ap);
int smap_asnprintf(char **pbuf, size_t *psize, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 3, 4)));
int smap_asprintf(char **pbuf, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));

#endif
