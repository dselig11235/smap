/* This file is part of Smap.
   Copyright (C) 2010, 2014, 2015 Sergey Poznyakoff

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
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <smap/stream.h>
#include <smap/diag.h>
#include <smap/module.h>
#include <smap/parseopt.h>
#include <smap/wordsplit.h>

#include <libguile.h>

#ifndef HAVE_SCM_T_OFF
typedef off_t scm_t_off;
#endif

static SCM
eval_catch_body(void *list)
{
	SCM pair = (SCM)list;
	return scm_apply_0(SCM_CAR(pair), SCM_CDR(pair));
}

static SCM
eval_catch_handler(void *data, SCM tag, SCM throw_args)
{
	scm_handle_by_message_noexit("idest", tag, throw_args);
	longjmp(*(jmp_buf*)data, 1);
}

struct scheme_exec_data {
	SCM (*handler) (void *data);
	void *data;
};

static SCM
scheme_safe_exec_body(void *data)
{
	struct scheme_exec_data *ed = data;
	return ed->handler (ed->data);
}

static int
guile_safe_exec(SCM (*handler) (void *data), void *data, SCM *result)
{
	jmp_buf jmp_env;
	struct scheme_exec_data ed;
	SCM res;

	if (setjmp(jmp_env))
		return 1;
	ed.handler = handler;
	ed.data = data;
	res = scm_c_catch(SCM_BOOL_T,
			  scheme_safe_exec_body, (void*)&ed,
			  eval_catch_handler, &jmp_env,
			  NULL, NULL);
	if (result)
		*result = res;
	return 0;
}


static char *
proc_name(SCM proc)
{
	return scm_to_locale_string(
		scm_symbol_to_string(scm_procedure_name(proc)));
}

static void
str_rettype_error(const char *name)
{
	smap_error("%s: invalid return type", name);
}


static int
guile_call_proc(SCM *result, SCM proc, SCM arglist)
{
	jmp_buf jmp_env;

	if (setjmp(jmp_env)) {
		char *name = proc_name(proc);
		smap_error("procedure `%s' failed", name);
		free(name);
		return 1;
	}

	*result = scm_c_catch(SCM_BOOL_T,
			      eval_catch_body, scm_cons(proc, arglist),
			      eval_catch_handler, &jmp_env,
			      NULL, NULL);
	return 0;
}

struct load_closure {
	char *filename;
	int argc;
	char **argv;
};

static SCM
load_path_handler(void *data)
{
	struct load_closure *lp = data;

	scm_set_program_arguments(lp->argc, lp->argv, lp->filename);
	scm_primitive_load_path(scm_from_locale_string(lp->filename));
	return SCM_UNDEFINED;
}

static int
guile_load_file(char *filename, char *args)
{
	int rc;
	struct load_closure lc;
	struct wordsplit ws;
	if (args) {
		if (wordsplit(args, &ws, WRDSF_DEFFLAGS)) {
			smap_error("guile: cannot parse command line: %s",
				   strerror(errno));
			return 1;
		}
		lc.argc = ws.ws_wordc;
		lc.argv = ws.ws_wordv;
	} else {
		lc.argc = 0;
		lc.argv = NULL;
	}
	lc.filename = filename;
	rc = guile_safe_exec(load_path_handler, &lc, NULL);
	if (lc.argc)
		wordsplit_free(&ws);
	return rc;
}

static SCM
argv_to_scm(int argc, char **argv)
{
	SCM scm_first = SCM_EOL, scm_last;

	for (; argc; argc--, argv++) {
		SCM new = scm_cons(scm_from_locale_string(*argv), SCM_EOL);
		if (scm_first == SCM_EOL)
			scm_last = scm_first = new;
		else {
			SCM_SETCDR(scm_last, new);
			scm_last = new;
		}
	}
	return scm_first;
}

static scm_t_bits scm_tc16_smap_output_port;
struct _guile_smap_output_port {
	smap_stream_t stream;
};

static SCM
_make_smap_output_port(smap_stream_t stream)
{
	struct _guile_smap_output_port *dp;
	SCM port;
	scm_port *pt;

	dp = scm_gc_malloc(sizeof (struct _guile_smap_output_port),
			   "smap-output_-port");
	dp->stream = stream;

	port = scm_new_port_table_entry(scm_tc16_smap_output_port);
	pt = SCM_PTAB_ENTRY(port);
	pt->rw_random = 0;
	SCM_SET_CELL_TYPE(port,
			  (scm_tc16_smap_output_port |
			   SCM_OPN | SCM_WRTNG | SCM_BUF0));
	SCM_SETSTREAM(port, dp);
	return port;
}

#define SMAP_OUTPUT_PORT(x) ((struct _guile_smap_output_port *) SCM_STREAM (x))

static SCM
_smap_output_port_mark(SCM port)
{
	return SCM_BOOL_F;
}

static void
_smap_output_port_flush(SCM port)
{
	scm_port *pt = SCM_PTAB_ENTRY(port);
	struct _guile_smap_output_port *dp = SMAP_OUTPUT_PORT(port);
	if (dp && pt->write_pos > pt->write_buf)
		smap_stream_write(dp->stream, "\n", 1, NULL);
}

static int
_smap_output_port_close(SCM port)
{
	struct _guile_smap_output_port *dp = SMAP_OUTPUT_PORT(port);

	if (dp) {
		_smap_output_port_flush(port);
		SCM_SETSTREAM(port, NULL);
		scm_gc_free(dp, sizeof(struct _guile_smap_output_port),
			    "smap-output-port");
	}
	return 0;
}

static scm_sizet
_smap_output_port_free(SCM port)
{
	_smap_output_port_close(port);
	return 0;
}

static int
_smap_output_port_fill_input(SCM port)
{
	return EOF;
}

static void
_smap_output_port_write(SCM port, const void *data, size_t size)
{
	struct _guile_smap_output_port *dp = SMAP_OUTPUT_PORT(port);
	smap_stream_write(dp->stream, data, size, NULL);
}

static scm_t_off
_smap_output_port_seek (SCM port, scm_t_off offset, int whence)
{
	return -1;
}

static int
_smap_output_port_print(SCM exp, SCM port, scm_print_state *pstate)
{
	scm_puts ("#<Smap output port>", port);
	return 1;
}

static void
_init_smap_output_port()
{
	scm_tc16_smap_output_port = scm_make_port_type("smap-output-port",
						_smap_output_port_fill_input,
						_smap_output_port_write);
	scm_set_port_mark (scm_tc16_smap_output_port, _smap_output_port_mark);
	scm_set_port_free (scm_tc16_smap_output_port, _smap_output_port_free);
	scm_set_port_print (scm_tc16_smap_output_port,
			    _smap_output_port_print);
	scm_set_port_flush (scm_tc16_smap_output_port,
			    _smap_output_port_flush);
	scm_set_port_close (scm_tc16_smap_output_port,
			    _smap_output_port_close);
	scm_set_port_seek (scm_tc16_smap_output_port,
			   _smap_output_port_seek);
}

static void
_add_load_path(char *path)
{
	SCM scm, path_scm;
	SCM *pscm;

	path_scm = SCM_VARIABLE_REF(scm_c_lookup("%load-path"));
	for (scm = path_scm; scm != SCM_EOL; scm = SCM_CDR(scm)) {
		SCM val = SCM_CAR(scm);
		if (scm_is_string(val)) {
			char *s = scm_to_locale_string(val);
			int res = strcmp(s, path);
			free(s);
			if (res == 0)
				return;
		}
	}

	pscm = SCM_VARIABLE_LOC(scm_c_lookup("%load-path"));
	*pscm = scm_append(scm_list_3(path_scm,
				      scm_list_1(scm_from_locale_string(path)),
				      SCM_EOL));
}

static int
set_load_path(const char *val)
{
	char *p;
	char *tmp = strdup(val);
	if (!tmp)
		return 1;
	for (p = strtok(tmp, ":"); p; p = strtok(NULL, ":"))
		_add_load_path(p);
	free(tmp);
	return 0;
}


static int guile_debug;
static char *guile_init_script;
static char *guile_init_args;
static char *guile_init_fun = "init";
static char *guile_load_path;

enum guile_proc_ind {
	init_proc, /* FIXME: this one is not yet used */
	done_proc, /* FIXME: this one too */
	open_proc,
	close_proc,
	query_proc,
	xform_proc
};
#define MAX_PROC (xform_proc+1)

static char *guile_proc_name[] = {
	"init",
	"done",
	"open",
	"close",
	"query",
	"xform"
};

typedef SCM guile_vtab[MAX_PROC];

struct _guile_database {
	guile_vtab vtab;
	const char *dbname;
	int argc;
	char **argv;
	int oport_inited;
	SCM handle;
};

static guile_vtab global_vtab;
SCM_GLOBAL_VARIABLE_INIT (sym_smap_debug_port, "smap-debug-port",
			  SCM_UNDEFINED);

static int
proc_name_to_index(const char *name)
{
	int i;
	for (i = 0; i < MAX_PROC; i++)
		if (strcmp(guile_proc_name[i], name) == 0)
			break;
	return i;
}


struct init_struct {
	const char *init_fun;
	const char *db_name;
};

static SCM
call_init_handler(void *data)
{
	struct init_struct *p = (struct init_struct *)data;
	SCM procsym = SCM_VARIABLE_REF(scm_c_lookup(p->init_fun));
	SCM arg;
	if (p->db_name)
		arg = scm_from_locale_string(p->db_name);
	else
		arg = SCM_BOOL_F;
	return scm_apply_0(procsym, scm_list_1(arg));
}

static int
init_vtab(const char *init_fun, const char *dbname, guile_vtab vtab)
{
	SCM res;
	struct init_struct istr;

	istr.init_fun = init_fun;
	istr.db_name = dbname;
	if (guile_safe_exec(call_init_handler, &istr, &res))
		return 1;

	if (!scm_is_pair(res) && res != SCM_EOL) {
		str_rettype_error(init_fun);
		return 1;
	}
	for (; res != SCM_EOL; res = SCM_CDR(res)) {
		int idx;
		char *ident;
		SCM name, proc;
		SCM car = SCM_CAR(res);
		if (!scm_is_pair(res)
		    || !scm_is_string(name = SCM_CAR(car))
		    || !scm_procedure_p(proc = SCM_CDR(car)))  {
			str_rettype_error(init_fun);
			return 1;
		}
		ident = scm_to_locale_string(name);
		idx = proc_name_to_index(ident);
		if (idx == MAX_PROC) {
			smap_error("%s: %s: unknown virtual function",
				   init_fun, ident);
			free(ident);
			return 1;
		}
		free(ident);
		vtab[idx] = proc;
	}
	return 0;
}

static struct smap_option init_option[] = {
	{ SMAP_OPTSTR(debug), smap_opt_bool, &guile_debug },
	{ SMAP_OPTSTR(init-script), smap_opt_string, &guile_init_script },
	{ SMAP_OPTSTR(init-args), smap_opt_string, &guile_init_args },
	{ SMAP_OPTSTR(load-path), smap_opt_string, &guile_load_path },
	{ SMAP_OPTSTR(init-fun), smap_opt_string, &guile_init_fun },
	{ NULL }
};


static int
guile_init(int argc, char **argv)
{
	SCM port;

	if (smap_parseopt(init_option, argc, argv, 0, NULL))
		return 1;
	scm_init_guile();
	scm_load_goops();
#include "guile.x"

	if (guile_debug) {
#ifdef GUILE_DEBUG_MACROS
		SCM_DEVAL_P = 1;
		SCM_BACKTRACE_P = 1;
		SCM_RECORD_POSITIONS_P = 1;
		SCM_RESET_DEBUG_MODE;
#endif
	}

	_init_smap_output_port();

	port = _make_smap_output_port(smap_error_str);
	if (port == SCM_BOOL_F) {
		smap_error("guile: cannot initialize error port");
		return 1;
	}
	scm_set_current_error_port(port);
	port = _make_smap_output_port(smap_debug_str);
	SCM_VARIABLE_SET(sym_smap_debug_port, port);

#ifdef ADDLOADPATH
	set_load_path(ADDLOADPATH);
#endif
	if (guile_load_path)
		set_load_path(guile_load_path);

	if (guile_init_script) {
		if (guile_load_file(guile_init_script, guile_init_args)) {
			smap_error("guile: cannot load init script %s",
				   guile_init_script);
			return 1;
		}

		if (guile_init_fun
		    && init_vtab(guile_init_fun, NULL, global_vtab))
			return 1;
	}
	return 0;
}

static smap_database_t
guile_init_db(const char *dbname, int argc, char **argv)
{
	struct _guile_database *db;
	int i;
	char *init_script = NULL;
	char *init_args = NULL;
	char *init_fun = guile_init_fun;

	struct smap_option db_option[] = {
		{ SMAP_OPTSTR(init-script), smap_opt_string, &init_script },
		{ SMAP_OPTSTR(init-args), smap_opt_string, &init_args },
		{ SMAP_OPTSTR(init-fun), smap_opt_string, &init_fun },
		{ NULL }
	};

	if (smap_parseopt(db_option, argc, argv, SMAP_PARSEOPT_PERMUTE, &i))
		return NULL;
	argc -= i;
	argv += i;

	if (init_script && guile_load_file(init_script, init_args)) {
		smap_error("guile: cannot load init script %s",
			   init_script);
		return NULL;
	}

	db = malloc(sizeof(*db));
	if (!db) {
		smap_error("guile: not enough memory");
		return NULL;
	}
	db->dbname = dbname;
	db->argc = argc;
	db->argv = argv;
	db->oport_inited = 0;
	db->handle = SCM_UNSPECIFIED;
	memcpy(db->vtab, global_vtab, sizeof(db->vtab));
	if (init_fun && init_vtab(init_fun, dbname, db->vtab)) {
		free(db);
		return NULL;
	}

	if (!db->vtab[query_proc] && !db->vtab[xform_proc]) {
		smap_error("guile: %s: neither %s nor %s are defined",
			   dbname, guile_proc_name[query_proc],
			   guile_proc_name[xform_proc]);
		free(db);
		return NULL;
	}

	return (smap_database_t)db;
}

static int
guile_free_db(smap_database_t hp)
{
	struct _guile_database *db = (struct _guile_database *)hp;
	free(db);
	return 0;
}

static int
guile_open(smap_database_t dp)
{
    struct _guile_database *db = (struct _guile_database *)dp;
    if (db->vtab[open_proc]) {
	    if (guile_call_proc(&db->handle, db->vtab[open_proc],
				scm_cons(scm_from_locale_string(db->dbname),
					 argv_to_scm(db->argc, db->argv))))
		    return 1;
	    if (db->handle == SCM_UNSPECIFIED)
		    return 1;
	    scm_gc_protect_object(db->handle);
    }
    return 0;
}

static int
guile_close(smap_database_t hp)
{
    struct _guile_database *db = (struct _guile_database *)hp;
    SCM res;

    if (db->vtab[close_proc]) {
	    if (guile_call_proc(&res, db->vtab[close_proc],
				scm_list_1(db->handle)))
		    return 1;
    }
    if (db->handle != SCM_UNSPECIFIED) {
	    scm_gc_unprotect_object(db->handle);
	    db->handle = SCM_UNSPECIFIED;
    }
    return 0;
}


static SCM
scm_from_smap_conninfo(struct smap_conninfo const *conninfo)
{
	return scm_list_2(scm_from_sockaddr(conninfo->src, conninfo->srclen),
			  scm_from_sockaddr(conninfo->dst, conninfo->dstlen));
}

static int
guile_query(smap_database_t dbp,
	    smap_stream_t ostr,
	    const char *map, const char *key,
	    struct smap_conninfo const *conninfo)
{
	struct _guile_database *db = (struct _guile_database *)dbp;
	SCM res, arg;

	if (!db->oport_inited) {
		SCM port = _make_smap_output_port(ostr);
		if (port == SCM_BOOL_F) {
			smap_error("guile: cannot initialize output port");
			return 1;
		}
		scm_set_current_output_port(port);
		db->oport_inited = 1;
	}
	arg = scm_append(scm_list_2(scm_list_3(db->handle,
					       scm_from_locale_string(map),
					       scm_from_locale_string(key)),
				    scm_from_smap_conninfo(conninfo)));
	if (guile_call_proc(&res, db->vtab[query_proc], arg))
		return 1;
	return res == SCM_BOOL_F;
}

static int
guile_xform(smap_database_t dbp,
	    struct smap_conninfo const *conninfo,
	    const char *input,
	    char **output)
{
	struct _guile_database *db = (struct _guile_database *)dbp;
	SCM res, arg;
	
	if (!db->vtab[xform_proc])
		return 1;
	
	arg = scm_append(scm_list_2(scm_list_2(db->handle,
					       scm_from_locale_string(input)),
				    scm_from_smap_conninfo(conninfo)));
	if (guile_call_proc(&res, db->vtab[xform_proc], arg))
		return 1;
	if (!scm_is_string(res))
		return 1;
	*output = scm_to_locale_string(res);
	return 0;
}

struct smap_module SMAP_EXPORT(guile, module) = {
	SMAP_MODULE_VERSION,
	SMAP_CAPA_QUERY|SMAP_CAPA_XFORM,
	guile_init,
	guile_init_db,
	guile_free_db,
	guile_open,
	guile_close,
	guile_query,
	guile_xform,
};
