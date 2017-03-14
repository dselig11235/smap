/* This file is part of Smap.
   Copyright (C) 2010, 2014 Sergey Poznyakoff

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

#ifndef __SMAP_MODULE_H
#define __SMAP_MODULE_H

#define __smap_s_cat3__(a,b,c) a ## b ## c
#define SMAP_EXPORT(module,name) __smap_s_cat3__(module,_LTX_,name)

#define SMAP_MODULE_VERSION 2
#define SMAP_CAPA_NONE 0
#define SMAP_CAPA_QUERY 0x0001
#define SMAP_CAPA_XFORM 0x0002
#define SMAP_CAPA_DEFAULT SMAP_CAPA_QUERY

typedef struct smap_database *smap_database_t;

struct sockaddr;

struct smap_conninfo {
	struct sockaddr const *src;
	int srclen;
	struct sockaddr const *dst;
	int dstlen;
};

struct smap_module {
	unsigned smap_version;
	unsigned smap_capabilities;
	int (*smap_init)(int argc, char **argv);
	smap_database_t (*smap_init_db)(const char *dbid,
					int argc, char **argv);
	int (*smap_free_db)(smap_database_t dbp);
	int (*smap_open) (smap_database_t hp);
	int (*smap_close) (smap_database_t hp);
	int (*smap_query)(smap_database_t dbp,
			  smap_stream_t ostr,
			  const char *map, const char *key,
			  struct smap_conninfo const *conninfo);
	int (*smap_xform)(smap_database_t dbp,
			  struct smap_conninfo const *conninfo,
			  const char *input,
			  char **output);
};

#endif
