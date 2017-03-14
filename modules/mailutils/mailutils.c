/* This file is part of Smap.
   Copyright (C) 2006-2010, 2014 Sergey Poznyakoff

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
#include <mailutils/mailutils.h>
#include <mailutils/cfg.h>
#include <mailutils/libcfg.h>
#include <mailutils/sys/stream.h>
#include <sysexits.h>
#include <smap/stream.h>
#include <smap/diag.h>
#include <smap/module.h>
#include <smap/parseopt.h>
#include <smap/wordsplit.h>

static char *dfl_positive_reply = "OK";
static char *dfl_negative_reply = "NOTFOUND";
static char *dfl_onerror_reply = "NOTFOUND";
static size_t dbgid;

typedef int (*query_fun_t)(smap_database_t dbp,
			   smap_stream_t ostr,
			   const char *map, const char *key,
			   struct smap_conninfo const *conninfo);

struct _mu_smap_db {
	const char *id;
	char *positive_reply;
	char *negative_reply;
	char *onerror_reply;
	char *config_file;
	int config_flags;
	query_fun_t qfn;
};

struct _mu_smap_result {
	const char *db;
	const char *map;
	const char *key;
	struct mu_auth_data *auth;
	mu_url_t url;
	mu_off_t mbsize;
	size_t msgsize;
	char *diag;
};

static void
_mu_smap_db_free(struct _mu_smap_db *db)
{
	free(db->positive_reply);
	free(db->negative_reply);
	free(db->onerror_reply);
	free(db);
}

static void
free_env(char **env)
{
	int i;
	for (i = 0; env[i]; i++)
		free(env[i]);
}

static char *
mkvar(const char *name, const char *val)
{
	char *ptr = malloc(strlen(name) + strlen(val) + 2);
	if (ptr) {
		strcpy(ptr, name);
		strcat(ptr, "=");
		strcat(ptr, val);
	}
	return ptr;
}

static int
mkenv(char **env, struct _mu_smap_result *res)
{
	int i = 0;
	struct mu_auth_data *auth = res->auth;
	char buf[512];
	
#define MKVAR(n, v)				\
	do {					\
		if (!(env[i++] = mkvar(n, v)))	\
			return 1;		\
	} while (0)
	
	MKVAR("db", res->db);
	MKVAR("key", res->key);
	MKVAR("map", res->map);
	if (auth) {
		MKVAR(MU_AUTH_NAME, auth->name);
		MKVAR(MU_AUTH_PASSWD, auth->passwd);
		snprintf(buf, sizeof buf, "%lu", (unsigned long) auth->uid);
		MKVAR(MU_AUTH_UID, buf);
		snprintf(buf, sizeof buf, "%lu", (unsigned long) auth->gid);
		MKVAR(MU_AUTH_GID, buf);
		MKVAR(MU_AUTH_GECOS, auth->gecos);
		MKVAR(MU_AUTH_DIR, auth->dir);
		MKVAR(MU_AUTH_SHELL, auth->shell);
		MKVAR(MU_AUTH_MAILBOX,
				 auth->mailbox ? auth->mailbox :
				   res->url ? mu_url_to_string(res->url) : "");
		snprintf(buf, sizeof buf, "%lu", (unsigned long) auth->quota);
		MKVAR(MU_AUTH_QUOTA, buf);
		snprintf(buf, sizeof buf, "%lu", (unsigned long) res->mbsize);
		MKVAR("mbsize", buf);
		snprintf(buf, sizeof buf, "%lu", (unsigned long) res->msgsize);
		MKVAR("msgsize", buf);
	}
	if (res->diag)
		MKVAR("diag", res->diag);

	env[i] = NULL;
	return 0;
}

static int
expand_reply_text(const char *arg, struct _mu_smap_result *res, char **repl)
{
	int rc;
	char *env[16];
	struct wordsplit ws;

	if (mkenv(env, res)) {
		mu_error("not enough memory");
		free_env(env);
		return 1;
	}
	
	ws.ws_env = (const char **) env;
	ws.ws_error = smap_error;
	rc = wordsplit(arg, &ws,
		       WRDSF_NOSPLIT |
		       WRDSF_NOCMD |
		       WRDSF_ENV |
		       WRDSF_ERROR |
		       WRDSF_SHOWERR);
	free_env(env);
	if (rc)
		return 1;
	*repl = ws.ws_wordv[0];
	ws.ws_wordv[0] = NULL;
	wordsplit_free(&ws);
	return 0;
}

static int
_mu_auth_query(smap_database_t dbp,
	       smap_stream_t ostr,
	       const char *map, const char *key,
	       struct smap_conninfo const *conninfo)
{
	struct _mu_smap_db *mdb = (struct _mu_smap_db *)dbp;
	struct mu_auth_data *auth = mu_get_auth_by_name(key);
	struct _mu_smap_result res;
	char *reply;
	int rc;
	
	res.db = mdb->id;
	res.map = map;
	res.key = key;
	res.auth = auth;
	res.mbsize = 0;
	res.msgsize = 0;
	res.diag = NULL;
	res.url = NULL;
	if (!auth)
		rc = expand_reply_text(mdb->negative_reply, &res, &reply);
	else {
		rc = expand_reply_text(mdb->positive_reply, &res, &reply);
		mu_auth_data_free(auth);
	}
	if (rc == 0) {
		smap_stream_printf(ostr, "%s\n", reply);
		free(reply);
	}
	return rc;
}

static int
mod_mailutils_query(smap_database_t dbp,
		    smap_stream_t ostr,
		    const char *map, const char *key,
		    struct smap_conninfo const *conninfo)
{
	struct _mu_smap_db *mdb = (struct _mu_smap_db *)dbp;
	return mdb->qfn(dbp, ostr, map, key, conninfo);
}

static int
switch_user_id(struct mu_auth_data *auth, int user)
{
	int rc;
	uid_t uid;

	if (!auth || auth->change_uid == 0)
		return 0;

	if (user)
		uid = auth->uid;
	else
		uid = 0;

	rc = setreuid(0, uid);
	if (rc < 0)
		mu_error("setreuid(0, %d): %s (r=%d, e=%d)",
			 uid, strerror(errno), getuid(), geteuid());
	return rc;
}

static int
checksize_user(struct _mu_smap_db *mdb, smap_stream_t ostr,
	       struct _mu_smap_result *res,
	       char **preply)
{
	int status, rc = 0;
	struct mu_auth_data *auth = res->auth;
	mu_mailbox_t mbox;
	
	status = mu_mailbox_create_default(&mbox, auth->mailbox);
	if (status) {
		res->diag = "local system error";
		mu_error("could not create mailbox `%s': %s",
			  auth->mailbox,  mu_strerror(status));
		return 0;
	}

	mu_mailbox_get_url(mbox, &res->url);

	status = mu_mailbox_open(mbox, MU_STREAM_READ);
	if (status) {
		res->diag = "local system error";
		mu_error("could not open mailbox `%s': %s",
			  auth->mailbox,  mu_strerror(status));
	} else {
		mu_off_t size;

		status = mu_mailbox_get_size(mbox, &size);
		if (status) {
			res->diag = "local system error";
			mu_error("could not get size for `%s': %s",
				  auth->mailbox, mu_strerror(status));
		} else {
			char *stat;

			res->mbsize = size;
			if (!auth->quota) {
				stat = mdb->positive_reply;
				res->diag = "NOQUOTA";
			} else if (size >= auth->quota) {
				stat = mdb->negative_reply;
				res->diag = "mailbox quota exceeded "
					    "for this recipient";
			} else if (res->msgsize
				   && size + res->msgsize >= auth->quota) {
				stat = mdb->negative_reply;
				res->diag = "message would exceed "
					    "maximum mailbox size "
					    "for this recipient";
			} else {
				stat = mdb->positive_reply;
				res->diag = "QUOTAOK";
			}
			rc = expand_reply_text(stat, res, preply);
		}
		mu_mailbox_close(mbox);
	}
	mu_mailbox_destroy(&mbox);
	return rc;
}

static int
checksize(struct _mu_smap_db *mdb, smap_stream_t ostr,
	  const char *user, struct _mu_smap_result *res,
	  char **preply)
{
	struct mu_auth_data *auth;
	int status;

	*preply = NULL;

	auth = mu_get_auth_by_name(user);
	if (!auth) {
		res->diag = "user not found";
		smap_debug(dbgid, 1, ("%s: user not found", user));
		return 0;
	}
	if (switch_user_id(auth, 1)) {
		res->diag = "local system error";
		return 0;
	}
	res->auth = auth;
	status = checksize_user(mdb, ostr, res, preply);
	switch_user_id(auth, 0);
	mu_auth_data_free(auth);
	res->auth = NULL;
	return status;
}

static int
_mu_mbq_query(smap_database_t dbp,
	      smap_stream_t ostr,
	      const char *map, const char *key,
	      struct smap_conninfo const *conninfo)
{
	struct _mu_smap_db *mdb = (struct _mu_smap_db *)dbp;
	char *user = strdup(key);
	char *p;
	size_t len;
	struct _mu_smap_result res;
	char *reply;
	int rc;
	
	memset(&res, 0, sizeof(res));
	res.db = mdb->id;
	res.map = map;
	res.key = key;

	len = strcspn(user, " \t");
	if (user[len]) {
		char *q;
		unsigned long n;

		user[len++] = 0;
		p = user + len;
		if (strncmp(p, "SIZE=", 5) == 0)
			p += 5;
		n = strtoul(p, &q, 10);
		if (*q == 0)
			res.msgsize = n;
		else
			smap_debug(dbgid, 1,
				   ("ignoring junk after %s", user + len));
	}

	rc = checksize(mdb, ostr, user, &res, &reply);

	if (!rc && !reply)
		rc = expand_reply_text(mdb->onerror_reply, &res, &reply);
	if (rc == 0) {
		smap_stream_printf(ostr, "%s\n", reply);
		free(reply);
	}
	free(user);
	return rc;
}



static const char *capa[] = {
	"auth",
	"common",
	"debug",
	"mailbox",
	"logging",
	NULL
};

static struct mu_cfg_param cfg_param[] = {
	{ "positive-reply", mu_cfg_string, &dfl_positive_reply, 0, NULL,
	  "set default positive reply text" },
	{ "negative-reply", mu_cfg_string, &dfl_negative_reply, 0, NULL,
	  "set default negative reply text" },
	{ "onerror-reply", mu_cfg_string, &dfl_onerror_reply, 0, NULL,
	  "set default error reply text" },
	{ NULL }
};


struct cap_buf {
	int err;
	char **capa;
	size_t numcapa;
	size_t maxcapa;
};

static int
cap_buf_init(struct cap_buf *bp)
{
	bp->err = 0;
	bp->numcapa = 0;
	bp->maxcapa = 2;
	bp->capa = calloc(bp->maxcapa, sizeof bp->capa[0]);
	if (!bp->capa) {
		mu_error ("%s", mu_strerror(errno));
		bp->err = 1;
		return 1;
	}
	bp->capa[0] = NULL;
	return 0;
}

static int
cap_buf_add(struct cap_buf *bp, char *str)
{
	if (bp->err)
		return 1;
	if (bp->numcapa == bp->maxcapa) {
		size_t n = bp->maxcapa * 2;
		char **p = realloc(bp->capa, n * sizeof bp->capa[0]);
		if (!p) {
			mu_error("%s", mu_strerror(errno));
			bp->err = 1;
			return 1;
		}
		bp->capa = p;
		bp->maxcapa = n;
	}
	bp->capa[bp->numcapa] = str;
	if (str)
		bp->numcapa++;
	return 0;
}

static void
cap_buf_free(struct cap_buf *bp)
{
	free(bp->capa);
}

static int
_reg_action(void *item, void *data)
{
	struct cap_buf *bp = data;
	return cap_buf_add(bp, item);
}

static int
init_cfg_capa()
{
	int i;
	struct cap_buf cb;
		
	for (i = 0; capa[i]; i++)
		mu_gocs_register_std(capa[i]);

	if (cap_buf_init(&cb) == 0) {
		mu_gocs_enumerate(_reg_action, &cb);
		cap_buf_add (&cb, NULL);
	}
	if (cb.err)
		return 1;
	mu_libcfg_init(cb.capa);
	cap_buf_free(&cb);
	return 0;
}

struct _smap_log_stream
{
	struct _mu_stream base;
	smap_stream_t transport;
};
	
static int
_smap_log_stream_write (struct _mu_stream *stream, const char *buf,
			size_t size, size_t *pret)
{
	struct _smap_log_stream *sp = (struct _smap_log_stream *)stream;
	return smap_stream_write(sp->transport, buf, size, pret);
}

static int
_smap_log_flush (struct _mu_stream *stream)
{
	struct _smap_log_stream *sp = (struct _smap_log_stream *)stream;
	return smap_stream_flush(sp->transport);
}

static int
_smap_log_stream_ioctl (struct _mu_stream *stream, int code, int opcode,
			void *arg)
{
	struct _smap_log_stream *sp = (struct _smap_log_stream *)stream;

	switch (code) {
	case MU_IOCTL_LOGSTREAM:
		switch (opcode) {
		case MU_IOCTL_LOGSTREAM_SET_SEVERITY:
			if (!arg)
				return EINVAL;
			smap_stream_flush(sp->transport);
			if (*(unsigned*) arg == MU_LOG_DEBUG)
				sp->transport = smap_debug_str;
			else
				sp->transport = smap_error_str;
			return 0;
		}
	}
	return ENOSYS;
}

static int
_smap_log_stream_create (mu_stream_t *pstr)
{
	struct _smap_log_stream *sp;

	sp = (struct _smap_log_stream *)
		_mu_stream_create(sizeof(*sp), MU_STREAM_WRITE);
	if (!sp)
		return ENOMEM;
	sp->base.write = _smap_log_stream_write;
	sp->base.flush = _smap_log_flush;
	sp->base.ctl = _smap_log_stream_ioctl;
	sp->transport = smap_error_str;
	*pstr = (mu_stream_t) sp;
	mu_stream_set_buffer(*pstr, smap_buffer_line, 1024);
	return 0;
}

static int
create_log_stream (mu_stream_t *pstream)
{
	int rc;
	mu_stream_t transport, str;

	rc = _smap_log_stream_create(&transport);
	if (rc) {
		smap_error("cannot create log stream: %s", mu_strerror(rc));
		return 1;
	} else {
		mu_stream_t flt;
		char *fltargs[3] = { "INLINE-COMMENT", };
		
		mu_asprintf(&fltargs[1], "%s: ", mu_program_name);
		fltargs[2] = NULL;
		rc = mu_filter_create_args(&flt, transport,
					   "INLINE-COMMENT",
					   2, (const char**)fltargs,
					   MU_FILTER_ENCODE, MU_STREAM_WRITE);
		free(fltargs[1]);
		if (rc) {
			smap_error("cannot open output filter stream: %s",
				   mu_strerror(rc));
			/* try to continue anyway */
		} else {
			mu_stream_unref(transport);
			transport = flt;
			mu_stream_set_buffer(transport, mu_buffer_line, 0);
		}

		rc = mu_log_stream_create(&str, transport);
		mu_stream_unref(transport);
		if (rc) {
			smap_error("cannot create mailutils logger stream: %s",
				   mu_strerror(rc));
			return 1;
		}
		*pstream = str;
	}
	return 0;
}


#ifndef MU_PARSE_CONFIG_PLAIN
# define MU_PARSE_CONFIG_PLAIN 0
#endif

static void
init_hints(struct mu_cfg_parse_hints *hints)
{
	hints->flags |= MU_PARSE_CONFIG_PLAIN | 
		        MU_CFG_PARSE_SITE_RCFILE | MU_CFG_PARSE_PROGRAM;
	hints->site_rcfile = mu_site_rcfile;
	hints->program = (char*) mu_program_name;
}

static int
mod_mailutils_init(int argc, char **argv)
{
	int rc;
	struct mu_cfg_tree *parse_tree = NULL;
	struct mu_cfg_parse_hints hints = { 0 };
	mu_stream_t logstr;
	
	struct smap_option init_option[] = {
		{ SMAP_OPTSTR(positive-reply), smap_opt_string,
		  &dfl_positive_reply },
		{ SMAP_OPTSTR(negative-reply), smap_opt_string,
		  &dfl_negative_reply },
		{ SMAP_OPTSTR(onerror-reply), smap_opt_string,
		  &dfl_onerror_reply },
		{ SMAP_OPTSTR(config-verbose), smap_opt_bitmask,
		  &hints.flags, { MU_PARSE_CONFIG_VERBOSE } },
		{ SMAP_OPTSTR(config-dump), smap_opt_bitmask,
		  &hints.flags, { MU_PARSE_CONFIG_DUMP } },
		{ NULL }
	};

	mu_set_program_name("smap-mailutils"); /**argv++*/
	/* Create MU log stream.  Reference it twice lest it gets released
	   during mu_gocs_flush (see below) */
	if (create_log_stream (&logstr))
		return 1;
	mu_stream_ref (logstr);
	mu_strerr = logstr;
	
	dbgid = smap_debug_alloc("mailutils");
	if (smap_parseopt(init_option, argc, argv, 0, NULL))
		return 1;

	init_hints(&hints);

	MU_AUTH_REGISTER_ALL_MODULES();
	/* register the formats.  */
	mu_register_all_mbox_formats();

	if (init_cfg_capa())
		return 1;
	
	rc = mu_cfg_parse_config(&parse_tree, &hints);
	if (rc == 0)
		rc = mu_cfg_tree_reduce(parse_tree, &hints, cfg_param, NULL);
	mu_gocs_flush();
	/* mu_gocs_flush may have reset the error stream, so restore it */
	mu_stream_unref(mu_strerr);
	mu_strerr = logstr;
	mu_cfg_destroy_tree(&parse_tree);
	
	return !!(rc || mu_cfg_error_count);
}

static smap_database_t
mod_mailutils_init_db(const char *dbid, int argc, char **argv)
{
	struct _mu_smap_db *db;
	char *positive_reply = NULL;
	char *negative_reply = NULL;
	char *onerror_reply = NULL;
#define MODE_AUTH 0
#define MODE_MBQ 1
	static const char *mode_choice[] = { "auth", "mbq", NULL };
	static query_fun_t qfn_tab[] = { _mu_auth_query, _mu_mbq_query };
	int mode = MODE_AUTH;
	char *config_file = NULL;
	int cfgflags = MU_PARSE_CONFIG_PLAIN;
	struct smap_option init_option[] = {
		{ SMAP_OPTSTR(mode), smap_opt_enum, &mode,
		  { enumstr: mode_choice } },
		{ SMAP_OPTSTR(positive-reply), smap_opt_string,
		  &positive_reply },
		{ SMAP_OPTSTR(negative-reply), smap_opt_string,
		  &negative_reply },
		{ SMAP_OPTSTR(onerror-reply), smap_opt_string,
		  &onerror_reply },
		{ SMAP_OPTSTR(config-file), smap_opt_string,
		  &config_file },
		{ SMAP_OPTSTR(config-verbose), smap_opt_bitmask,
		  &cfgflags, { MU_PARSE_CONFIG_VERBOSE } },
		{ SMAP_OPTSTR(config-dump), smap_opt_bitmask,
		  &cfgflags, { MU_PARSE_CONFIG_DUMP } },
		{ NULL }
	};

	if (smap_parseopt(init_option, argc, argv, 0, NULL))
		return NULL;

	db = malloc(sizeof(*db));
	if (!db) {
		mu_error("not enough memory");
		return NULL;
	}
	db->id = dbid;
	db->positive_reply = positive_reply ?
			       positive_reply : strdup(dfl_positive_reply);
	db->negative_reply = negative_reply ?
			       negative_reply : strdup(dfl_negative_reply);
	db->onerror_reply = onerror_reply ?
			       onerror_reply : strdup(dfl_onerror_reply);
	db->config_file = config_file;
	db->config_flags = cfgflags;
	if (!db->positive_reply || !db->negative_reply || !db->onerror_reply) {
		_mu_smap_db_free(db);
		return NULL;
	}
	db->qfn = qfn_tab[mode];
	return (smap_database_t)db;
}

static int
mod_mailutils_free_db(smap_database_t dbp)
{
	struct _mu_smap_db *db = (struct _mu_smap_db *)dbp;
	_mu_smap_db_free(db);
	return 0;
}

static int
mod_mailutils_open(smap_database_t dbp)
{
	struct _mu_smap_db *db = (struct _mu_smap_db *)dbp;

	if (db->config_file) {
		struct mu_cfg_parse_hints hints = { db->config_flags };
		mu_cfg_tree_t *tree = NULL;
		int rc;

		init_hints(&hints);
		if (init_cfg_capa())
			return 1;

		mu_cfg_error_count = 0;
		mu_cfg_parser_verbose = 0;
		if (db->config_flags & MU_PARSE_CONFIG_VERBOSE)
			mu_cfg_parser_verbose++;
		if (db->config_flags & MU_PARSE_CONFIG_DUMP)
			mu_cfg_parser_verbose++;
		rc = mu_cfg_parse_file(&tree, db->config_file,
				       db->config_flags);
		
		if (rc) {
			mu_error("error parsing %s: %s", db->config_file,
				 mu_strerror(rc));
			return 1;
		}

		mu_cfg_tree_postprocess(tree, &hints);
		
		rc = mu_cfg_tree_reduce(tree, &hints, cfg_param, NULL);
		mu_gocs_flush();
		mu_cfg_destroy_tree(&tree);

		if (rc || mu_cfg_error_count) {
			mu_error("errors parsing %s", db->config_file);
			return 1;
		}
	}
	return 0;
}

struct smap_module SMAP_EXPORT(mailutils, module) = {
	SMAP_MODULE_VERSION,
	SMAP_CAPA_DEFAULT,
	mod_mailutils_init,
	mod_mailutils_init_db,
	mod_mailutils_free_db,
	mod_mailutils_open, /* smap_open */
	NULL, /* smap_close */
	mod_mailutils_query,
	NULL, /* smap_xform */
};
