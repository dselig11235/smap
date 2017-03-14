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
#include <config.h>
#endif

#include <string.h>
#include "smap/kwtab.h"

int
smap_kwtab_nametotok_len(struct smap_kwtab *kwtab, const char *str,
			 size_t len, int *pres)
{
	for (; kwtab->name; kwtab++) {
		size_t kwlen = strlen(kwtab->name);
		if (kwlen == len && memcmp(kwtab->name, str, len) == 0) {
			*pres = kwtab->tok;
			return 0;
		}
	}
	return 1;
}

int
smap_kwtab_nametotok(struct smap_kwtab *kwtab, const char *str, int *pres)
{
	for (; kwtab->name; kwtab++) {
		if (strcmp(kwtab->name, str) == 0) {
			*pres = kwtab->tok;
			return 0;
		}
	}
	return 1;
}

int
smap_kwtab_toktoname(struct smap_kwtab *kwtab, int tok, const char **pres)
{
	for (; kwtab->name; kwtab++)
		if (kwtab->tok == tok) {
			*pres = kwtab->name;
			return 0;
		}
	return 1;
}
