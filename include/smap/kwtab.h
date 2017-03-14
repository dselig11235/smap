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

#ifndef __SMAP_KWTAB_H
#define __SMAP_KWTAB_H

struct smap_kwtab {
	char *name;
	int tok;
};

int smap_kwtab_nametotok_len(struct smap_kwtab *kwtab, const char *str,
			     size_t len, int *pres);
int smap_kwtab_nametotok(struct smap_kwtab *kwtab, const char *str, int *pres);
int smap_kwtab_toktoname(struct smap_kwtab *kwtab, int tok, const char **pres);

#endif
