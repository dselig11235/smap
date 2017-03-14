/* This file is part of Smap.
   Copyright (C) 2008, 2010, 2014 Sergey Poznyakoff

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

#ifndef __SMAP_PARSEOPT_H
#define __SMAP_PARSEOPT_H

#include <stdlib.h>

enum smap_opt_type {
	smap_opt_null,
	smap_opt_bool,
	smap_opt_bitmask,
	smap_opt_bitmask_rev,
	smap_opt_long,
	smap_opt_string,
	smap_opt_enum,
	smap_opt_const,
	smap_opt_const_string
};

struct smap_option {
	const char *name;
	size_t len;
	enum smap_opt_type type;
	void *data;
	union {
		long value;
		const char **enumstr;
	} v;
	int (*func) (struct smap_option const *, const char *, char **);
};

#define SMAP_OPTSTR(s) #s, (sizeof(#s) - 1)

#define SMAP_PARSEOPT_PARSE_ARGV0 0x01
#define SMAP_PARSEOPT_PERMUTE     0x02

#define SMAP_DELIM_EQ   0x00
#define SMAP_DELIM_WS   0x01
#define SMAP_DELIM_MASK 0x01
#define SMAP_IGNORECASE 0x10

#define SMAP_PARSE_SUCCESS 0
#define SMAP_PARSE_NOENT   1
#define SMAP_PARSE_INVAL   2

int smap_parseline(struct smap_option const *opt, const char *line, int delim,
		   char **errmsg);
int smap_parseopt(struct smap_option const *opt, int argc, char **argv,
		  int flags, int *index);

#endif
