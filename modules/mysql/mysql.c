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

#include <mysql/mysql.h>
#include <smap/diag.h>
#include <smap/module.h>
#include <smap/parseopt.h>
#include <smap/stream.h>
#include <smap/wordsplit.h>

#define MDB_OPEN  0x01
#define MDB_DEFDB 0x02

struct mod_mysql_db {
	int flags;
	unsigned refcnt;
	MYSQL mysql;
	const char *name;
	char *config_file;
	char *config_group;
	char *ssl_ca;
	char *host;
	char *user;
	char *password;
	char *database;
	long port;
	char *socket;
	char *template;
	char *positive_reply;
	char *negative_reply;
	char *onerror_reply;
};

static size_t dbgid;
static struct mod_mysql_db def_db;

static MYSQL *
moddb_handle(struct mod_mysql_db *db)
{
	if (db->flags & MDB_DEFDB)
		return &def_db.mysql;
	return &db->mysql;
}

static const char *
moddb_positive_reply(struct mod_mysql_db *db)
{
	return db->positive_reply ?
		db->positive_reply :
		(def_db.positive_reply ? def_db.positive_reply : "OK");
}

static const char *
moddb_negative_reply(struct mod_mysql_db *db)
{
	return db->negative_reply ?
		db->negative_reply :
		(def_db.negative_reply ? def_db.negative_reply : "NOTFOUND");
}

static const char *
moddb_onerror_reply(struct mod_mysql_db *db)
{
	return db->onerror_reply ?
		db->onerror_reply :
		(def_db.onerror_reply ? def_db.onerror_reply : "NOTFOUND");
}

static int
opendb(struct mod_mysql_db *db)
{
	if (db->flags & MDB_OPEN) {
		db->refcnt++;
		return 0;
	}

	if (!mysql_init(&db->mysql)) {
		smap_error("%s: not enough memory", db->name);
		return 1;
	}
		
	if (db->config_file)
		mysql_options(&db->mysql, MYSQL_READ_DEFAULT_FILE,
			       db->config_file);
	if (db->config_group)
		mysql_options (&db->mysql, MYSQL_READ_DEFAULT_GROUP,
			       db->config_group);

	if (db->ssl_ca)
		mysql_ssl_set (&db->mysql, NULL, NULL, db->ssl_ca,
			       NULL, NULL);

	if (!mysql_real_connect(&db->mysql,
				db->host,
				db->user,
				db->password,
				db->database,
				db->port,
				db->socket, CLIENT_MULTI_RESULTS)) {
		smap_error("%s: failed to connect to database, error: %s",
			   db->name, mysql_error(&db->mysql));
		mysql_close(&db->mysql);
		return 1;
	}
	db->refcnt++;
	db->flags |= MDB_OPEN;
	return 0;
}

static void
closedb(struct mod_mysql_db *db)
{
	if ((db->flags & MDB_OPEN) && --db->refcnt == 0) {
		mysql_close(&db->mysql);
		db->flags &= ~MDB_OPEN;
	}
}

static void
freedb(struct mod_mysql_db *db)
{
	free(db->config_file);
	free(db->config_group);
	free(db->ssl_ca);
	free(db->host);
	free(db->user);
	free(db->password);
	free(db->database);
	free(db->socket);
	free(db->template);
	free(db->positive_reply);
	free(db->negative_reply);
	free(db->onerror_reply);
}

static int
dbdeclared(struct mod_mysql_db *db)
{
	return db->config_file ||
	       db->config_group ||
	       db->ssl_ca ||
	       db->host ||
	       db->user ||
	       db->password ||
	       db->database ||
	       db->port ||
	       db->socket;
}


static int
mod_init(int argc, char **argv)
{
	int rc;
	
	struct smap_option init_option[] = {
		{ SMAP_OPTSTR(config-file), smap_opt_string,
		  &def_db.config_file },
		{ SMAP_OPTSTR(config-group), smap_opt_string,
		  &def_db.config_group },
		{ SMAP_OPTSTR(ssl-ca), smap_opt_string,
		  &def_db.ssl_ca },
		{ SMAP_OPTSTR(host), smap_opt_string,
		  &def_db.host },
		{ SMAP_OPTSTR(user), smap_opt_string,
		  &def_db.user },
		{ SMAP_OPTSTR(password), smap_opt_string,
		  &def_db.password },
		{ SMAP_OPTSTR(database), smap_opt_string,
		  &def_db.database },
		{ SMAP_OPTSTR(port), smap_opt_long,
		  &def_db.port },
		{ SMAP_OPTSTR(socket), smap_opt_string,
		  &def_db.socket },
		
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
	dbgid = smap_debug_alloc("mysql");
	rc = smap_parseopt(init_option, argc, argv, 0, NULL);
	if (rc)
		return rc;
	def_db.flags = 0;
	return 0;
}

static smap_database_t
mod_init_db(const char *dbid, int argc, char **argv)
{
	struct mod_mysql_db *db;
	char *positive_reply = NULL;
	char *negative_reply = NULL;
	char *onerror_reply = NULL;
	char *query = NULL;
	char *config_file = NULL;
	char *config_group = NULL;
	char *ssl_ca = NULL;
	char *host = NULL;
	char *user = NULL;
	char *password = NULL;
	char *database = NULL;
	long port = 0;
	char *socket = NULL;
	int flags = 0;
	struct smap_option init_option[] = {
		{ SMAP_OPTSTR(defaultdb), smap_opt_bitmask,
		  &flags, { MDB_DEFDB } },
		{ SMAP_OPTSTR(config-file), smap_opt_string,
		  &config_file },
		{ SMAP_OPTSTR(config-group), smap_opt_string,
		  &config_group },
		{ SMAP_OPTSTR(ssl-ca), smap_opt_string,
		  &ssl_ca },
		{ SMAP_OPTSTR(host), smap_opt_string,
		  &host },
		{ SMAP_OPTSTR(user), smap_opt_string,
		  &user },
		{ SMAP_OPTSTR(password), smap_opt_string,
		  &password },
		{ SMAP_OPTSTR(database), smap_opt_string,
		  &database },
		{ SMAP_OPTSTR(port), smap_opt_long,
		  &port },
		{ SMAP_OPTSTR(socket), smap_opt_string,
		  &socket },
		
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

	if (smap_parseopt(init_option, argc, argv, 0, NULL))
		return NULL;
	
	db = calloc(1, sizeof(*db));
	if (!db) {
		smap_error("%s: not enough memory", dbid);
		return NULL;
	}
	
	db->flags = flags;
	db->name = dbid;
	db->config_file = config_file;
	db->config_group = config_group;
	db->ssl_ca = ssl_ca;
	db->host = host;
	db->user = user;
	db->password = password;
	db->database = database;
	db->port = port;
	db->socket = socket;
	db->template = query;
	db->positive_reply = positive_reply;
	db->negative_reply = negative_reply;
	db->onerror_reply = onerror_reply;

	if (!dbdeclared(db))
		db->flags |= MDB_DEFDB;

	return (smap_database_t) db;
}

static int
mod_free_db(smap_database_t dbp)
{
	struct mod_mysql_db *db = (struct mod_mysql_db *)dbp;
	freedb(db);
	free(db);
	return 0;
}

static int
mod_open(smap_database_t dbp)
{
	struct mod_mysql_db *db = (struct mod_mysql_db *)dbp;
	if (db->flags & MDB_DEFDB)
		return opendb(&def_db);
	return opendb(db);
}

static int
mod_close(smap_database_t dbp)
{
	struct mod_mysql_db *db = (struct mod_mysql_db *)dbp;
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

static int
fill_env(MYSQL *mysql, char **vartab, const char **intab, char ***penv)
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
		if (mysql) {
			size_t ilen = strlen(val);
			size_t olen = 2 * ilen + 1;
			if (!buf) {
				buf = malloc(olen);
				if (!buf) {
					rc = 1;
					break;
				}
				size = olen;
			} else if (olen > size) {
				char *np = realloc(buf, olen);
				if (!np) {
					rc = 1;
					break;
				}
				buf = np;
				size = olen;
			}

			mysql_real_escape_string(mysql, buf, val, ilen);
			val = buf;
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
create_query_env(struct mod_mysql_db *db,
		 const char *map, const char *key,
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
	
	if (fill_env(NULL, vartab, intab, penv))
		return 1;
	if (fill_env(moddb_handle(db), vartab, intab, pqenv)) {
		free_env(*penv);
		return 1;
	}
	return 0;
}
	
static int
do_query(struct mod_mysql_db *db, char **env, MYSQL_RES **pres)
{
	struct wordsplit ws;
	int rc;
	MYSQL *mysql = moddb_handle(db);
	
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
	
	rc = mysql_query(mysql, ws.ws_wordv[0]);

	if (rc) {
		smap_error("%s: query failed: %s",
			   db->name, mysql_error(&db->mysql));
		smap_error("%s: failed query: %s",
			   db->name, ws.ws_wordv[0]);
	}
	wordsplit_free(&ws);
	if (rc)
		return 1;
	
	*pres = mysql_store_result(mysql);
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
do_positive_reply(struct mod_mysql_db *db, 
		  smap_stream_t ostr,
		  char ***penv,
		  MYSQL_RES *result)
{
	unsigned nfld;
	char **env;
	size_t i, j;
	MYSQL_FIELD *fields;
	MYSQL_ROW row;
	
	row = mysql_fetch_row(result);
	nfld = mysql_num_fields(result);
	
	env = *penv;
	for (i = 0; env[i]; i++)
		;
	env = realloc(env, (nfld + i + 1) * sizeof(env[0]));
	if (!env) { 
		smap_error("not enough memory");
		return 1;
	}
	*penv = env;

	fields = mysql_fetch_fields(result);
	for (j = 0; j < nfld; j++) {
		char *p = format_envar(fields[j].name, row[j]);
		env[i + j] = p;
		if (!p) {
			smap_error("not enough memory");
			return 1;
		}
	}
	env[i + j] = NULL;

	return send_reply(ostr, moddb_positive_reply(db), env);
}

/* Consume additional result sets */
static void
flush_result(struct mod_mysql_db *db)
{
	MYSQL *mysql = moddb_handle(db);
	while (mysql_next_result(mysql) == 0) {
		MYSQL_RES *result = mysql_store_result(mysql);
		if (!result)
			break;
		if (mysql_field_count(mysql))
			while (mysql_fetch_row(result))
				;
		mysql_free_result(result);
	}
}

static int
mod_query(smap_database_t dbp,
	  smap_stream_t ostr,
	  const char *map, const char *key,
	  struct smap_conninfo const *conninfo)
{
	struct mod_mysql_db *db = (struct mod_mysql_db *)dbp;
	MYSQL_RES *res;
	int rc;
	char **env, **qenv;

	if (create_query_env(db, map, key, conninfo, &env, &qenv))
		return 1;
	
	rc = do_query(db, qenv, &res);
	free_env(qenv);
	
	if (rc) {
		rc = send_reply(ostr, moddb_onerror_reply(db), env);
	} else if (res) {
		unsigned nrow = mysql_num_rows(res);
		unsigned ncol = mysql_num_fields(res);
	
		smap_debug(dbgid, 1,
			   ("query returned %u columns in %u rows",
			    ncol, nrow));
		if (nrow > 0)
			rc = do_positive_reply(db, ostr, &env, res);
		else	
			rc = send_reply(ostr, moddb_negative_reply(db),
					env);
		mysql_free_result(res);
		flush_result(db);
	} else
		rc = send_reply(ostr, moddb_negative_reply(db), env);
	free_env(env);
	return rc;
}

struct smap_module SMAP_EXPORT(mysql, module) = {
	SMAP_MODULE_VERSION,
	SMAP_CAPA_QUERY,
	mod_init,
	mod_init_db,
	mod_free_db,
	mod_open,
	mod_close,
	mod_query,
	NULL /* smap_xform */
};

