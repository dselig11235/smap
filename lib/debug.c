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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "smap/diag.h"

static size_t debug_max;
static size_t debug_next;
struct debug_level {
	const char *name;
	int level;
};
static struct debug_level *debug_level;
static int global_debug_level;

int
smap_debug_np(size_t mod, size_t lev)
{
	if (mod == (size_t)-1 || mod >= debug_next)
		return 0;
	if (debug_level[mod].level == 0)
		return global_debug_level >= lev;
	return debug_level[mod].level >= lev;
}

int
smap_debug_sp(const char *name, size_t lev)
{
	size_t mod = smtp_debug_locate(name);
	return smap_debug_np(mod, lev);
}

size_t
smtp_debug_locate_n(const char *name, size_t len)
{
	size_t i;
	for (i = 0; i < debug_next; i++)
		if (strncmp(debug_level[i].name, name, len) == 0)
			return i;
	return (size_t)-1;
}

size_t
smtp_debug_locate(const char *name)
{
	return smtp_debug_locate_n(name, strlen(name));
}

size_t
smap_debug_alloc_n(const char *name, size_t n)
{
	size_t ret;

	ret = smtp_debug_locate_n(name, n);
	if (ret != (size_t)-1)
		return ret;

	if (debug_next >= debug_max) {
		struct debug_level *newp;
		size_t newmax;
		if (debug_max == 0)
			newmax = 128;
		else {
			newmax = debug_max *= 2;
			if (newmax < debug_max)
				return (size_t)-1;
		}
		newp = realloc(debug_level, sizeof(debug_level[0])*newmax);
		if (!newp)
			return (size_t)-1;
		debug_max = newmax;
		debug_level = newp;
	}
	debug_level[debug_next].name = strdup(name);
	if (!debug_level[debug_next].name)
		return (size_t)-1;
	debug_level[debug_next].level = global_debug_level;
	return debug_next++;
}

size_t
smap_debug_alloc(const char *name)
{
	return smap_debug_alloc_n(name, strlen(name));
}

void
smap_debug_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	smap_stream_vprintf(smap_debug_str, fmt, ap);
	smap_stream_write(smap_debug_str, "\n", 1, NULL);
	va_end(ap);
}

int
smap_debug_set(const char *spec)
{
	char *p;
	unsigned n, lev;

	if (isdigit(spec[0])) {
		n = strtoul(spec, &p, 0);
		if (!*p) {
			global_debug_level = n;
			return 0;
		}

		if (*p != '.')
			return -1;
	} else {
		size_t i = strcspn(spec, ".");
		n = smap_debug_alloc_n(spec, i);
		if (spec[i])
			p = (char*)(spec + i);
		else
			p = ".100";
	}

	if (n == (size_t)-1 || n >= debug_next)
		return -1;

	lev = strtoul(p + 1, &p, 0);
	if (*p)
		return -1;
	debug_level[n].level = lev;
	return 0;
}
