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
#include <regex.h>
#include <smap/diag.h>
#include <smap/module.h>
#include <smap/parseopt.h>
#include <smap/stream.h>
#include <smap/wordsplit.h>
#include "transform.h"

static char *def_positive_reply = "OK ${xform}";
static char *def_negative_reply = "NOTFOUND";
static char *def_onerror_reply = NULL;
static int def_cflags = REG_EXTENDED;

struct sed_slist_entry {
	struct sed_slist_entry *next;
	char *str;
	size_t len;
};

struct sed_slist {
	struct sed_slist_entry *head, *tail;
	size_t total;
};

static void
slist_init(struct sed_slist *slist)
{
	slist->head = slist->tail = NULL;
	slist->total = 0;
}

static void
slist_free(struct sed_slist *slist)
{
	struct sed_slist_entry *ent;

	for (ent = slist->head; ent; ) {
		struct sed_slist_entry *next = ent->next;
		free(ent);
		ent = next;
	}
	slist_init(slist);
}

static struct sed_slist_entry *
slist_new_entry(const char *str, size_t len)
{
	struct sed_slist_entry *p = malloc(sizeof(*p) + len);
	if (p) {
		p->str = (char*)(p + 1);
		p->len = len;
		memcpy(p->str, str, len);
	}
	return p;
}
	

static int
sed_slist_append(void *data, const char *str, size_t len)
{
	struct sed_slist *slist = (struct sed_slist *)data;
	struct sed_slist_entry *ent = slist_new_entry(str, len);
	if (!ent)
		return 1;
	ent->next = NULL;
	if (slist->tail)
		slist->tail->next = ent;
	else
		slist->head = ent;
	slist->tail = ent;
	slist->total += len;
	return 0;
}
	
static int
sed_slist_reduce(void *data, char **pret)
{
	struct sed_slist *slist = (struct sed_slist *)data;
	char *output;

	if (slist->total == 0)
		output = strdup ("");
	else {
		struct sed_slist_entry *ent;
		size_t off = 0;
		
		output = malloc(slist->total + 1);
		if (!output)
			return 1;
		for (ent = slist->head; ent; ) {
			struct sed_slist_entry *next = ent->next;
			memcpy(output + off, ent->str, ent->len);
			off += ent->len;
			free(ent);
			ent = next;
		}
		output[off] = 0;
		slist_init(slist);
	}
	if (!output)
		return 1;
	*pret = output;
	return 0;
}
		

static int
sed_init(int argc, char **argv)
{
	static struct smap_option init_option[] = {
		{ SMAP_OPTSTR(icase), smap_opt_bitmask,
		  &def_cflags, { REG_ICASE } },
		{ SMAP_OPTSTR(extended), smap_opt_bitmask,
		  &def_cflags, { REG_EXTENDED } },
		{ SMAP_OPTSTR(positive-reply), smap_opt_string,
		  &def_positive_reply },
		{ SMAP_OPTSTR(negative-reply), smap_opt_string,
		  &def_negative_reply },
		{ SMAP_OPTSTR(onerror-reply), smap_opt_string,
		  &def_onerror_reply },
		{ NULL }
	};
	return smap_parseopt(init_option, argc, argv, 0, NULL);
}


struct sed_db {
	struct transform tr;
	struct sed_slist slist;
	char *positive_reply;
	char *negative_reply;
	char *onerror_reply;
};

static smap_database_t
sed_init_db(const char *dbid, int argc, char **argv)
{
	int i;
	struct sed_db *db;
	int cflags = def_cflags;
	char *positive_reply = def_positive_reply;
	char *negative_reply = def_negative_reply;
	char *onerror_reply = def_onerror_reply;

	struct smap_option init_option[] = {
		{ SMAP_OPTSTR(icase), smap_opt_bitmask,
		  &cflags, { REG_ICASE } },
		{ SMAP_OPTSTR(extended), smap_opt_bitmask,
		  &cflags, { REG_EXTENDED } },
		{ SMAP_OPTSTR(positive-reply), smap_opt_string,
		  &positive_reply },
		{ SMAP_OPTSTR(negative-reply), smap_opt_string,
		  &negative_reply },
		{ SMAP_OPTSTR(onerror-reply), smap_opt_string,
		  &onerror_reply },
		{ NULL }
	};

	if (smap_parseopt(init_option, argc, argv, SMAP_PARSEOPT_PERMUTE, &i))
		return NULL;

	db = malloc(sizeof(*db));
	if (!db) {
		smap_error("not enough memory");
		return NULL;
	}

	transform_init (&db->tr);
	db->tr.tr_append = sed_slist_append;
	db->tr.tr_reduce = sed_slist_reduce;
	db->positive_reply = positive_reply;
	db->negative_reply = negative_reply;
	db->onerror_reply = onerror_reply;
	slist_init(&db->slist);
	for (; i < argc; i++) {
		if (transform_compile_incr(&db->tr, argv[i], cflags)) {
			transform_perror(&db->tr, smap_error_str);
			smap_stream_write(smap_error_str, "\n", 1, NULL);
			transform_free(&db->tr);
			free(db);
			return NULL;
		}
	}
	return (smap_database_t) db;
}

static int
sed_free_db(smap_database_t dbp)
{
	struct sed_db *db = (struct sed_db *)dbp;
	transform_free(&db->tr);
	slist_free(&db->slist);
	free(db);
	return 0;
}


static int
sed_xform(smap_database_t dbp,
	  struct smap_conninfo const *conninfo,
	  const char *input,
	  char **output)
{
	struct sed_db *db = (struct sed_db *)dbp;
	
	if (transform_string(&db->tr, input, &db->slist, output)) {
		transform_perror(&db->tr, smap_error_str);
		smap_stream_write(smap_error_str, "\n", 1, NULL);
		return 1;
	}
	return 0;
}

static char *
format_env(const char *var, const char *val)
{
	char *p = malloc(strlen(var) + strlen(val) + 2);
	if (!p) {
		smap_error("not enough memory");
		return NULL;
	}
	strcpy(p, var);
	strcat(p, "=");
	strcat(p, val);
	return p;
}

static int
sed_reply(smap_stream_t ostr, const char *reply,
	  const char *map, const char *key, const char *xform)
{
	int rc;
	struct wordsplit ws;
	char *env[4];

	if ((env[0] = format_env("map", map)) == NULL)
		return 1;
	if ((env[1] = format_env("key", key)) == NULL) {
		free(env[0]);
		return 1;
	}
	if (xform) {
		if ((env[2] = format_env("xform", xform)) == NULL) {
			free(env[0]);
			free(env[1]);
			return 1;
		}
	} else
		env[2] = 0;
	env[3] = 0;

	ws.ws_env = (const char **) env;
	ws.ws_error = smap_error;
	rc = wordsplit(reply, &ws,
		       WRDSF_NOSPLIT |
		       WRDSF_NOCMD |
		       WRDSF_ENV |
		       WRDSF_ERROR |
		       WRDSF_SHOWERR);
	if (rc == 0)
		smap_stream_printf(ostr, "%s\n", ws.ws_wordv[0]);
	wordsplit_free(&ws);
	free(env[0]);
	free(env[1]);
	free(env[2]);
	return rc;
}

static int
sed_query(smap_database_t dbp,
	  smap_stream_t ostr,
	  const char *map, const char *key,
	  struct smap_conninfo const *conninfo)
{
	struct sed_db *db = (struct sed_db *)dbp;
	char *output;
	char *repl;
	int rc;
	
	if (transform_string(&db->tr, key, &db->slist, &output)) {
		transform_perror(&db->tr, smap_error_str);
		smap_stream_write(smap_error_str, "\n", 1, NULL);
		if (db->onerror_reply) {
			return sed_reply(ostr, db->onerror_reply, map, key,
					 NULL);
		}
		return 1;
	}

	repl = strcmp(key, output) ?
		   db->positive_reply : db->negative_reply;
	rc = sed_reply(ostr, repl, map, key, output);

	slist_free(&db->slist);
	free(output);
	return rc;
}

struct smap_module SMAP_EXPORT(sed, module) = {
	SMAP_MODULE_VERSION,
	SMAP_CAPA_QUERY|SMAP_CAPA_XFORM,
	sed_init,
	sed_init_db,
	sed_free_db,
	NULL, /* smap_open */
	NULL, /* smap_close */
	sed_query,
	sed_xform
};



