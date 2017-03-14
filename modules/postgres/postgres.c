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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libpq-fe.h>

#include <smap/diag.h>
#include <smap/module.h>
#include <smap/parseopt.h>
#include <smap/stream.h>
#include <smap/wordsplit.h>

#define MDB_OPEN  0x01
#define MDB_DEFDB 0x02

struct modpg_db {
	int flags;
	unsigned refcnt;
	const char *name;
	PGconn *pgconn;
	char *conninfo;
	char *template;
	char *positive_reply;
	char *negative_reply;
	char *onerror_reply;
};

static size_t dbgid;
static struct modpg_db def_db;

static PGconn *
modpg_handle(struct modpg_db *db)
{
	if (db->flags & MDB_DEFDB)
		return def_db.pgconn;
	return db->pgconn;
}

static const char *
modpg_positive_reply(struct modpg_db *db)
{
	return db->positive_reply ?
		db->positive_reply :
		(def_db.positive_reply ? def_db.positive_reply : "OK");
}

static const char *
modpg_negative_reply(struct modpg_db *db)
{
	return db->negative_reply ?
		db->negative_reply :
		(def_db.negative_reply ? def_db.negative_reply : "NOTFOUND");
}

static const char *
modpg_onerror_reply(struct modpg_db *db)
{
	return db->onerror_reply ?
		db->onerror_reply :
		(def_db.onerror_reply ? def_db.onerror_reply : "NOTFOUND");
}

static int
opendb(struct modpg_db *db)
{
	if (db->flags & MDB_OPEN) {
		db->refcnt++;
		return 0;
	}

	db->pgconn = PQconnectdb(db->conninfo);
	if (!db->pgconn) {
		smap_error("%s: out of memory", db->name);
		return 1;
	}
	
	if (PQstatus(db->pgconn) == CONNECTION_BAD) {
		smap_error("%s: cannot connect: %s", db->name,
			   PQerrorMessage(db->pgconn));
		PQfinish(db->pgconn);
		db->pgconn = NULL;
		return 1;
	}
	db->refcnt++;
	db->flags |= MDB_OPEN;
	return 0;
}

static void
closedb(struct modpg_db *db)
{
	if ((db->flags & MDB_OPEN) && --db->refcnt == 0) {
		PQfinish(db->pgconn);
		db->pgconn = NULL;
		db->flags &= ~MDB_OPEN;
	}
}

static void
freedb(struct modpg_db *db)
{
	free(db->conninfo);
	free(db->template);
	free(db->positive_reply);
	free(db->negative_reply);
	free(db->onerror_reply);
}
	

static size_t
modpg_quoted_length(const char *str)
{
	size_t len = 0;
	int quote = 0;
	
	for (; *str; str++) {
		if (*str == ' ') {
			len++;
			quote = 1;
		} else if (*str == '\'' || *str == '\\') {
			len += 2;
			quote = 1;
		} else
			len++;
	}
	return len;
}

static char *
create_conninfo(int argc, char **argv)
{
	int i;
	size_t total = 0;
	char *conninfo, *p;
	
	for (i = 0; i < argc; i++) {
		size_t len;

		p = strrchr(argv[i], '=');
		if (!p)
			len = strlen(argv[i]);
		else
			len = p - argv[i] + 1 + modpg_quoted_length(p + 1);
		total += len + 1;
	}

	conninfo = malloc(total);
	if (!conninfo)
		return NULL;
	
	p = conninfo;
	for (i = 0; i < argc; i++) {
		char *q;
		
		for (q = argv[i]; *q; ) {
			if ((*p++ = *q++) == '=')
				break;
		}

		if (*q) {
			if (q[strcspn(q, " \t\\'")]) {
				*p++ = '\'';
				while (*q) {
					if (*q == '\\' || *q == '\t')
						*p++ = '\\';
					*p++ = *q++;
				}
				*p++ = '\'';
			}
			while (*q)
				*p++ = *q++;
		}
		*p++ = ' ';
	}
	p[-1] = 0;
	return conninfo;
}

static int
modpg_init(int argc, char **argv)
{
	int i;
	int rc;
	
	dbgid = smap_debug_alloc("postgres");
	struct smap_option init_option[] = {
		{ SMAP_OPTSTR(query), smap_opt_string,
		  &def_db.template },
		{ SMAP_OPTSTR(positive-reply), smap_opt_string,
		  &def_db.positive_reply },
		{ SMAP_OPTSTR(negative-reply), smap_opt_string,
		  &def_db.negative_reply },
		{ SMAP_OPTSTR(onerror-reply), smap_opt_string,
		  &def_db.onerror_reply },
		{ NULL }
	};
	rc = smap_parseopt(init_option, argc, argv, SMAP_PARSEOPT_PERMUTE, &i);
	if (rc)
		return rc;
	if (i < argc) {
		def_db.conninfo = create_conninfo(argc - i, argv + i);
		if (!def_db.conninfo) {
			smap_error("out of memory");
			return 1;
		}
		def_db.flags = 0;
		def_db.name = "postgres";
	}
	return 0;
}

static smap_database_t
modpg_init_db(const char *dbid, int argc, char **argv)
{
	struct modpg_db *db;
	char *positive_reply = NULL;
	char *negative_reply = NULL;
	char *onerror_reply = NULL;
	char *query = NULL;
	int flags = 0;
	int i;
	
	struct smap_option init_option[] = {
		{ SMAP_OPTSTR(query), smap_opt_string,
		  &query },
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
	db = calloc(1, sizeof(*db));
	if (!db) {
		smap_error("%s: not enough memory", dbid);
		return NULL;
	}
	
	if (i < argc) {
		db->conninfo = create_conninfo(argc - i, argv + i);
		if (!db->conninfo) {
			smap_error("out of memory");
			free(db);
			return NULL;
		}
	} else
		flags |= MDB_DEFDB;

	db->flags = flags;
	db->name = dbid;
	db->template = query;
	db->positive_reply = positive_reply;
	db->negative_reply = negative_reply;
	db->onerror_reply = onerror_reply;

	return (smap_database_t) db;
}

static int
modpg_free_db(smap_database_t dbp)
{
	struct modpg_db *db = (struct modpg_db *)dbp;
	freedb(db);
	return 0;
}

static int
modpg_open(smap_database_t dbp)
{
	struct modpg_db *db = (struct modpg_db *)dbp;
	if (db->flags & MDB_DEFDB)
		return opendb(&def_db);
	return opendb(db);
}

static int
modpg_close(smap_database_t dbp)
{
	struct modpg_db *db = (struct modpg_db *)dbp;
	if (db->flags & MDB_DEFDB)
		closedb(&def_db);
	closedb(db);
	return 0;
}


static char *
format_envar(const char *var, const char *val)
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

static void
free_env(char **env)
{
	int i;
	for (i = 0; env[i]; i++)
		free(env[i]);
	free(env);
}

#define INIT_ENV_SIZE 4

static size_t
quoted_length(const char *str, int *pquote)
{
	int quote = 0;
	size_t len = strlen(str);
	
	for (; *str; str++) {
		if (*str == '\\' || *str == '\t' || *str == '\'') {
			len++;
			quote = 1;
		}
	}
	*pquote = quote;
	return len + 1;
}

static void
quote_copy(char *buf, const char *str)
{
	while (*str) {
		if (*str == '\\' || *str == '\t' || *str == '\'')
			*buf++ = '\\';
		*buf++ = *str++;
	}
	*buf = 0;
}

static int
fill_env(int escape, char **vartab, const char **intab, char ***penv)
{
	int rc = 0;
	int i;
	char **env;
	char *buf = NULL;
	size_t size = 0;
		
	env = calloc(INIT_ENV_SIZE + 1, sizeof(*env));
	if (!env) {
		smap_error("not enough memory");
		return 1;
	}

	for (i = 0; i < INIT_ENV_SIZE; i++) {
		const char *val = intab[i];

		if (!val)
			continue;
		if (escape) {
			int quote;
			size_t len = quoted_length(val, &quote);
			if (quote) {
				if (!buf) {
					buf = malloc(len);
					if (!buf) {
						rc = 1;
						break;
					}
					size = len;
				} else if (len > size) {
					char *np = realloc(buf, len);
					if (!np) {
						rc = 1;
						break;
					}
					buf = np;
					size = len;
				}

				quote_copy(buf, val);
				val = buf;
			}
		}
		env[i] = format_envar(vartab[i], val);
		if (!env[i]) {
			rc = 1;
			break;
		}
	}
	
	if (rc) {
		free_env(env);
		free(buf);
		smap_error("not enough memory");
	} else
		*penv = env;
	return rc;
}
		
static int
create_query_env(const char *map, const char *key,
		 struct smap_conninfo const *conninfo,
		 char ***penv, char ***pqenv)
{
	static char *vartab[] = { "map", "key", "src", "dst" };
	static const char *intab[INIT_ENV_SIZE];
	struct sockaddr_in *s_in;
	
	intab[0] = map;
	intab[1] = key;
	if (conninfo && conninfo->src->sa_family == AF_INET) {
		s_in = (struct sockaddr_in *)conninfo->src;
		intab[2] = inet_ntoa(s_in->sin_addr);
	} else
		intab[2] = NULL;
	if (conninfo && conninfo->dst->sa_family == AF_INET) {
		s_in = (struct sockaddr_in *)conninfo->dst;
		intab[3] = inet_ntoa(s_in->sin_addr);
	} else
		intab[3] = NULL;
	
	if (fill_env(0, vartab, intab, penv))
		return 1;
	if (fill_env(1, vartab, intab, pqenv)) {
		free_env(*penv);
		return 1;
	}
	return 0;
}

static int
do_query(struct modpg_db *db, char **env, PGresult **pres)
{
	struct wordsplit ws;
	int rc = 0;
	PGconn *pgconn = modpg_handle(db);
	PGresult *res;
	
	ws.ws_env = (const char **) env;
	ws.ws_error = smap_error;
	rc = wordsplit(db->template, &ws,
		       WRDSF_NOSPLIT |
		       WRDSF_NOCMD |
		       WRDSF_ENV |
		       WRDSF_ERROR |
		       WRDSF_SHOWERR);
	if (rc)
		return 1;

 	smap_debug(dbgid, 1,
		   ("running query: %s", ws.ws_wordv[0]));

	res = PQexec(pgconn, ws.ws_wordv[0]);
	if (res == NULL)
		rc = 1;
	else {
		ExecStatusType stat;
		stat = PQresultStatus(res);
		if (stat != PGRES_COMMAND_OK && stat != PGRES_TUPLES_OK)
			rc = 1;
	}
	if (rc) {
		smap_error("%s: query failed: %s",
			   db->name, PQerrorMessage(pgconn));
		smap_error("%s: failed query: %s",
			   db->name, ws.ws_wordv[0]);
	}
	wordsplit_free(&ws);
	if (rc)
		return 1;
	*pres = res;
	return 0;
}

static int
send_reply(smap_stream_t ostr, const char *template, char **env)
{
	struct wordsplit ws;
	int rc;
	
	ws.ws_env = (const char **) env;
	ws.ws_error = smap_error;
	rc = wordsplit(template, &ws,
		       WRDSF_NOSPLIT |
		       WRDSF_NOCMD |
		       WRDSF_ENV |
		       WRDSF_ERROR |
		       WRDSF_SHOWERR);
	if (rc) {
		smap_error("cannot format reply");
		wordsplit_free(&ws);
		return 1;
	}

 	smap_debug(dbgid, 1, ("reply: %s", ws.ws_wordv[0]));
	smap_stream_printf(ostr, "%s\n", ws.ws_wordv[0]);
	wordsplit_free(&ws);
	return 0;
}

static int
do_positive_reply(struct modpg_db *db, 
		  smap_stream_t ostr,
		  char ***penv,
		  PGresult *res)
{
	size_t nfld = PQnfields(res);
	char **env;
	size_t i, j;

	env = *penv;
	for (i = 0; env[i]; i++)
		;
	env = realloc(env, (nfld + i + 1) * sizeof(env[0]));
	if (!env) { 
		smap_error("not enough memory");
		return 1;
	}
	*penv = env;

	for (j = 0; j < nfld; j++) {
		char *p = format_envar(PQfname(res, j), PQgetvalue(res, 0, j));
		env[i + j] = p;
		if (!p) {
			smap_error("not enough memory");
			return 1;
		}
	}
	env[i + j] = NULL;

	return send_reply(ostr, modpg_positive_reply(db), env);
}
		
static int
modpg_query(smap_database_t dbp,
	    smap_stream_t ostr,
	    const char *map, const char *key,
	    struct smap_conninfo const *conninfo)
{
	struct modpg_db *db = (struct modpg_db *)dbp;
	PGresult *res;
	int rc;
	char **env, **qenv;
	
	if (create_query_env(map, key, conninfo, &env, &qenv))
		return 1;
	
	rc = do_query(db, qenv, &res);
	free_env(qenv);
	if (rc)
		rc = send_reply(ostr, modpg_onerror_reply(db), env);
	else {
		size_t ntuples = PQntuples(res);
		smap_debug(dbgid, 1,
			   ("query returned %u columns in %u rows",
			    PQnfields(res), ntuples));
		if (ntuples)
			rc = do_positive_reply(db, ostr, &env, res);
		else	
			rc = send_reply(ostr, modpg_negative_reply(db),	env);
	}
	free_env(env);
	return rc;
}

struct smap_module SMAP_EXPORT(postgres, module) = {
	SMAP_MODULE_VERSION,
	SMAP_CAPA_QUERY,
	modpg_init,
	modpg_init_db,
	modpg_free_db,
	modpg_open,
	modpg_close,
	modpg_query,
	NULL /* smap_xform */
};

