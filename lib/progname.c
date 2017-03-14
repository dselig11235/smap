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
#include <string.h>
#include "smap/diag.h"

char *smap_progname = "[progname is not set, please report]";

void
smap_set_program_name(char *name)
{
	char *p = strrchr(name, '/');
	if (p)
		p++;
	else
		p = name;
	if (strncmp(p, "lt-", 3) == 0)
		p += 3;
	smap_progname = p;
}
