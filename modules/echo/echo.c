/* This file is part of Smap.
   Copyright (C) 2006-2007, 2010, 2014 Sergey Poznyakoff

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <smap/stream.h>
#include <smap/diag.h>
#include <smap/module.h>

struct echo_database {
	int argc;
	char **argv;
};

static smap_database_t
echo_init_db(const char *dbid, int argc, char **argv)
{
	struct echo_database *db = malloc(sizeof(*db));
	if (!db) {
		smap_error("not enough memory");
		return NULL;
	}
	db->argc = argc - 1;
	db->argv = argv + 1;
	return (smap_database_t) db;
}

static int
echo_free_db(smap_database_t dbp)
{
	free(dbp);
	return 0;
}

static int
echo_query(smap_database_t dbp,
	   smap_stream_t ostr,
	   const char *map, const char *key,
	   struct smap_conninfo const *conninfo)
{
	struct echo_database *edb = (struct echo_database *) dbp;
	int i;

	for (i = 0; i < edb->argc; i++) {
		if (i)
			smap_stream_write(ostr, " ", 1, NULL);
		smap_stream_write(ostr, edb->argv[i], strlen(edb->argv[i]),
				  NULL);
	}
	smap_stream_write(ostr, "\n", 1, NULL);
	return 0;
}

struct smap_module SMAP_EXPORT(echo, module) = {
	SMAP_MODULE_VERSION,
	SMAP_CAPA_DEFAULT,
	NULL, /* smap_init */
	echo_init_db,
	echo_free_db,
	NULL, /* smap_open */
	NULL, /* smap_close */
	echo_query,
	NULL  /* smap_xform */
};
