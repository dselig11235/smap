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
#include <string.h>
#include "common.h"
#include "smap/diag.h"

void
emalloc_die(size_t size)
{
	smap_error("out of memory (allocating %lu bytes)",
		   (unsigned long) size);
	abort();
}

void *
emalloc(size_t size)
{
	void *p = malloc(size);
	if (!p)
		emalloc_die(size);
	return p;
}

void *
ecalloc(size_t nmemb, size_t size)
{
	void *p = calloc(nmemb, size);
	if (!p)
		emalloc_die(size);
	return p;
}

void *
erealloc(void *ptr, size_t newsize)
{
	void *p = realloc(ptr, newsize);
	if (!p)
		emalloc_die(newsize);
	return p;
}

char *
estrdup(const char *s)
{
	return strcpy(emalloc(strlen(s) + 1), s);
}
