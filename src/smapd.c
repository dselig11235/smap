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

#include "smapd.h"
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifndef SYSCONFDIR
# define SYSCONFDIR "/etc"
#endif

char *config_file = SYSCONFDIR "/smapd.conf";
int foreground;
unsigned idle_timeout = 600;
int inetd_mode;
int lint_mode;
char *pidfile;
struct privinfo global_privs;

#define OPT_INT 0
#define OPT_STR 1

struct optcache {
	struct optcache *next;
	int type;
	void *ptr;
	union {
		int ival;
		char *sval;
	} v;
};

static struct optcache *opthead, *opttail;

static struct optcache *
optcache_new(int type)
{
	struct optcache *p = emalloc(sizeof(*p));
	p->type = type;
	if (opttail)
		opttail->next = p;
	else
		opthead = p;
	opttail = p;
	return p;
}

static void
optcache_int(int *opt, int val)
{
	struct optcache *p = optcache_new(OPT_INT);
	p->ptr = opt;
	p->v.ival = val;
}

static void
optcache_str(char **opt, char *val)
{
	struct optcache *p = optcache_new(OPT_STR);
	p->ptr = opt;
	p->v.sval = val;
}

static void
optcache_flush()
{
	struct optcache *p;

	for (p = opthead; p; ) {
		struct optcache *next = p->next;
		switch (p->type) {
		case OPT_INT:
			*(int*)p->ptr = p->v.ival;
			break;
		case OPT_STR:
			*(char**)p->ptr = p->v.sval;
			break;
		}
		free(p);
		p = next;
	}
	opthead = opttail = NULL;
}


void
smap_trimnl(char *buf)
{
	size_t len = strlen(buf);
	if (len > 0 && buf[len-1] == '\n')
		buf[--len] = 0;
}

int
smap_loop(smap_stream_t stream, const char *id, struct smap_conninfo *conninfo)
{
	char *buf = NULL;
	size_t bufsize = 0;
	size_t len;
	char *key;

	/* Read input: */
	while (1) {
		int rc;

		alarm(idle_timeout);
		rc = smap_stream_getline(stream, &buf, &bufsize, &len);
		alarm(0);

		if (rc) {
			smap_error("read error: %s",
				   smap_stream_strerror(stream, rc));
			break;
		}
		if (len == 0)
			break;
		smap_trimnl(buf);
		key = strchr(buf, ' ');
		if (!key) {
			smap_error("protocol error: missing map name");
			exit(1);
		}
		*key++ = 0;

		dispatch_query(id, conninfo, stream, buf, key);
	}
	/* Cleanup and exit */
	free(buf);
	close_databases();
	return 0;
}

int
smap_session_server(const char *id, int fd,
		    struct sockaddr const *sa, socklen_t salen,
		    void *server_data, void *srvman_data)
{
	int rc;
	smap_stream_t stream;
	struct smap_conninfo ci;
	struct privinfo *pi = server_data;

	ci.src = sa;
	ci.srclen = salen;
	smap_srvman_get_sockaddr(id, &ci.dst, &ci.dstlen);

	if (pi) {
		if (getgid() == 0) {
			if (switch_to_privs(pi))
				return EX_UNAVAILABLE;
		} else if (pi->uid)
			debug(DBG_SMAP, 1,
			      ("%s: ignoring server privilege settings", id));
	}
	rc = smap_sockmap_stream_create(&stream, fd, SMAP_STREAM_NO_CLOSE);
	if (rc) {
		smap_error("cannot create socket stream: %s",
			   strerror(rc));
		return EX_UNAVAILABLE;
	}
	if (smap_debug_np(DBG_SMAP, 10)) {
		char *pfx[] = { "C", "S" };
		int idx = DBG_SMAP;
		smap_stream_ioctl(stream, SMAP_IOCTL_SET_DEBUG_IDX, &idx);
		smap_stream_ioctl(stream, SMAP_IOCTL_SET_DEBUG_PFX, pfx);
	}

	rc = smap_loop(stream, id, &ci);
	/*smap_stream_close(stream);*/
	smap_stream_destroy(&stream);
	return rc;
}


static int restart;         /* Set to 1 if restart is requested */

static RETSIGTYPE
sig_stop(int sig)
{
	smap_srvman_stop();
}

static RETSIGTYPE
sig_restart(int sig)
{
	restart = 1;
	smap_srvman_stop();
}



static int
expand_srv_groups(smap_server_t srv, void *data)
{
	struct privinfo *pi = data;
	if (pi) {
		privinfo_expand_user_groups(pi);
		if (pi->uid) {
			uid_t uid;
			gid_t gid;
			
			smap_server_get_owner(srv, &uid, &gid);
			if (gid == (gid_t)-1) {
				if (uid == (uid_t)-1)
					uid = pi->uid;
				smap_server_set_owner(srv, uid, pi->gids[0]);
			} else
				privinfo_add_grpgid(pi, gid);
		}
	}
	return 0;
}

void
smap_daemon(int argc, char **argv)
{
	smap_error(_("%s (%s) started"), smap_progname, PACKAGE_STRING);
	
	if (!foreground) {
		if (daemon(0, 0) < 0) {
			smap_error("failed to become a daemon");
			exit(EX_UNAVAILABLE);
		}
	}

	if (getgid() == 0) {
		privinfo_expand_user_groups(&global_privs);
		if (switch_to_privs(&global_privs))
			exit(EX_UNAVAILABLE);
	} else if (global_privs.uid)
		debug(DBG_SMAP, 1, ("ignoring master privilege settings"));

	if (pidfile) {
		FILE *fp = fopen(pidfile, "w");
		if (!fp) {
			smap_error("cannot open pidfile %s for writing: %s",
				   pidfile, strerror(errno));
		} else {
			fprintf(fp, "%lu\n", (unsigned long) getpid());
			fclose(fp);
		}
	}

	signal(SIGTERM, sig_stop);
	signal(SIGQUIT, sig_stop);
	if (argv[0][0] != '/') {
		smap_error(_("smap started "
			     "using relative file name; "
			     "restart (SIGHUP) will not work"));
		signal(SIGHUP, sig_stop);
	} else
		signal(SIGHUP, sig_restart);
	signal(SIGINT, sig_stop);

	smap_srvman_iterate_data(expand_srv_groups);
	if (smap_srvman_open()) {
		smap_error("no servers configured; exiting");
		exit(EX_CONFIG);
	}
	smap_srvman_run(NULL);
	smap_srvman_shutdown();
	smap_srvman_free();
	if (pidfile && unlink(pidfile))
		smap_error("failed to remove pidfile %s: %s",
			   pidfile, strerror(errno));
	if (restart) {
		smap_error(_("smapd restarting"));
		close_fds_above(1);
		execv(argv[0], argv);
		smap_log_init();
		smap_error(_("cannot restart: %s"), strerror(errno));
	} else
		smap_error(_("smapd terminating"));
}

void
smap_inet_server()
{
	all_addr_t clt_addr, srv_addr;
	socklen_t slen;
	struct smap_conninfo ci;
	int fds[2] = { 0, 1 };
	int rc;
	smap_stream_t stream;

	slen = sizeof(srv_addr);
	if (getsockname(0, &srv_addr.sa, &slen) == -1)
		smap_error("gethostname: %s", strerror(errno));
	else {
		ci.dst = &srv_addr.sa;
		ci.dstlen = slen;
	}
	slen = sizeof(clt_addr);
	if (getpeername(0, &clt_addr.sa, &slen) == -1)
		smap_error("getpeername: %s", strerror(errno));
	else {
		ci.src = &clt_addr.sa;
		ci.srclen = slen;
	}

	rc = smap_sockmap_stream_create2(&stream, fds, SMAP_STREAM_NO_CLOSE);
	if (rc) {
		smap_error("cannot create socket stream: %s",
			   strerror(rc));
		exit(EX_UNAVAILABLE);
	}
	rc = smap_loop(stream, smap_progname, &ci);
	smap_stream_destroy(&stream);
	if (rc)
		exit(EX_UNAVAILABLE);
}

static int
parse_mode_string(const char *str, mode_t *pmode)
{
	int i;
	mode_t mode = 0;
#define PARSECHAR(str, c, v)			\
	do {					\
		switch (*str++) {		\
                case c:				\
			x |= v;			\
			break;			\
                case '-':			\
			break;                  \
		default:                        \
			return 1;               \
		}                               \
        } while (0)

	if (strchr("01234567", str[0])) {
		char *p;
		unsigned long n = strtoul(str, &p, 8);
		if (*p)
			return 1;
		*pmode = n;
		return 0;
	}

	for (i = 0; i < 3; i++) {
		mode_t x = 0;
		PARSECHAR(str, 'r', 4);
		PARSECHAR(str, 'w', 2);
		PARSECHAR(str, 'x', 1);
		mode = (mode << 3) | x;
	}
	if (*str)
		return 1;
	*pmode = mode;
	return 0;
}


#define CFG_GETNUM(str, val)						\
	do {								\
		char *__p;						\
		unsigned long __n = strtoul(str, &__p, 0);		\
		if (*__p) {						\
			smap_error("%s:%u: invalid number (near %s)",	\
				   cfg_file_name, cfg_line, __p);	\
			return 1;					\
		}							\
		if ((val = __n) != __n) {				\
			smap_error("%s:%u: number out of range: %s",	\
				   cfg_file_name, cfg_line, __p);	\
			return 1;					\
		}							\
	}  while (0)

static int
bool_invert(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	*kw->ival = !*kw->ival;
	return 0;
}

static int
set_syslog_facility(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	if (cfg_chkargc(wordc, 2, 2))
		return 1;

	if (syslog_fname_to_n(wordv[1], kw->ival)) {
		smap_error("%s:%u: unrecognized facility: %s", 
			   cfg_file_name, cfg_line, wordv[1]);
		return 1;
	}
	return 0;
}

static int
cfg_debug(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	int i;

	if (cfg_chkargc(wordc, 2, 2))
		return 1;
	for (i = 1; i < wordc; i++) {
		if (smap_debug_set(wordv[i])) {
			smap_error("%s:%u: invalid debug specification: %s",
				   cfg_file_name, cfg_line, wordv[i]);
			return 1;
		}
	}
	return 0;
}




static int
cfg_backlog(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	unsigned n;
	smap_server_t srv = data;

	if (cfg_chkargc(wordc, 2, 2))
		return 1;
	CFG_GETNUM(wordv[1], n);
	smap_server_set_backlog(srv, n);
	return 0;
}

static int
cfg_server_flag(smap_server_t srv, int wordc, char **wordv, int flag)
{
	int n;

	if (cfg_chkargc(wordc, 2, 2))
		return 1;
	if (cfg_parse_bool(wordv[1], &n))
		return 1;
	smap_server_set_flags(srv, flag,
			      n ? srvman_bitop_set : srvman_bitop_clr);
	return 0;
}

static int
cfg_reuseaddr(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	smap_server_t srv = data;
	return cfg_server_flag(srv, wordc, wordv, SRV_KEEP_EXISTING);
}

static int
cfg_single_process(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	smap_server_t srv = data;
	return cfg_server_flag(srv, wordc, wordv, SRV_SINGLE_PROCESS);
}

static int
cfg_max_children(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	smap_server_t srv = data;
	size_t n;

	if (cfg_chkargc(wordc, 2, 2))
		return 1;
	CFG_GETNUM(wordv[1], n);
	smap_server_set_max_children(srv, n);
	return 0;
}

static int
_privinfo_free(void *data)
{
	struct privinfo *pi = data;
	free(pi->gids);
	free(pi);
	return 0;
}

static struct privinfo *
get_server_privinfo(smap_server_t srv)
{
	struct privinfo *pi;
	pi = smap_server_get_data_ptr(srv);
	if (!pi) {
		pi = ecalloc(1, sizeof(*pi));
		smap_server_set_data(srv, pi, _privinfo_free);
	}
	return pi;
}

static int
cfg_srv_user(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	smap_server_t srv = data;
	struct privinfo *pi;

	if (cfg_chkargc(wordc, 2, 2))
		return 1;

	pi = get_server_privinfo(srv);
	privinfo_set_user(pi, wordv[1]);
	return 0;
}

static int
cfg_srv_group(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	int i;
	smap_server_t srv = data;
	struct privinfo *pi;

	if (cfg_chkargc(wordc, 2, 0))
		return 1;

	pi = get_server_privinfo(srv);
	for (i = 1; i < wordc; i++)
		privinfo_add_grpnam(pi, wordv[i]);
	return 0;
}

static int
cfg_srv_allgroups(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	smap_server_t srv = data;
	struct privinfo *pi;

	if (cfg_chkargc(wordc, 2, 2))
		return 1;
	pi = get_server_privinfo(srv);
	pi->allgroups = 1;
	return 0;
}

static int
cfg_srv_chown(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	smap_server_t srv = data;
	char *user, *group, *endp;
	size_t off;
	uid_t uid = (uid_t)-1;
	uid_t gid = (gid_t)-1;
	
	if (cfg_chkargc(wordc, 2, 2))
		return 1;
	user = wordv[1];
	off = strcspn(user, ".:");
	if (user[off]) {
		user[off++] = 0;
		group = user + off;
	} else
		group = NULL;

	if (user && *user) {
		struct passwd *pw = getpwnam(user);
		if (pw) {
			uid = pw->pw_uid;
			if (!group)
				gid = pw->pw_gid;
		} else {
			unsigned long n = strtoul(user, &endp, 0);
			if (*endp) {
				smap_error("%s:%u: no such user",
					   cfg_file_name, cfg_line);
				return 1;
			}
			uid = n;
		}
	}

	if (group && *group) {
		struct group *gr = getgrnam(group);
		if (gr)
			gid = gr->gr_gid;
		else {
			unsigned long n = strtoul(group, &endp, 0);
			if (*endp) {
				smap_error("%s:%u: no such group",
					   cfg_file_name, cfg_line);
				return 1;
			}
			gid = n;
		}
	}

	if (uid != (uid_t)-1 && gid != (gid_t)-1) {
		struct privinfo *pi = get_server_privinfo(srv);
		privinfo_add_grpgid(pi, gid);
	}
	
	smap_server_set_owner(srv, uid, gid);
	
	return 0;
}

static int
cfg_srv_socket_mode(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	smap_server_t srv = data;
	mode_t mode;
	
	if (cfg_chkargc(wordc, 2, 2))
		return 1;
	if (parse_mode_string(wordv[1], &mode)) {
		smap_error("%s:%u: invalid file mode",
			   cfg_file_name, cfg_line);
		return 1;
	}
	smap_server_set_mode(srv, mode);
	return 0;
}

static struct cfg_kw server_kwtab[] = {
	{ "end", KWT_EOF },
	{ "backlog", KWT_FUN, NULL, NULL, NULL, cfg_backlog },
	{ "reuseaddr", KWT_FUN, NULL, NULL, NULL, cfg_reuseaddr },
	{ "max-children", KWT_FUN, NULL, NULL, NULL, cfg_max_children },
	{ "single-process", KWT_FUN, NULL, NULL, NULL, cfg_single_process },
	{ "user",  KWT_FUN, NULL, NULL, NULL, cfg_srv_user },
	{ "group", KWT_FUN, NULL, NULL, NULL, cfg_srv_group },
	{ "allgroups", KWT_BOOL, NULL, NULL, NULL, cfg_srv_allgroups },
	{ "socket-owner", KWT_FUN, NULL, NULL, NULL, cfg_srv_chown },
	{ "socket-mode", KWT_FUN, NULL, NULL, NULL, cfg_srv_socket_mode },
	{ NULL }
};

static int
cfg_server(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	smap_server_t srv;

	if (cfg_chkargc(wordc, 3, 4))
		return 1;
	srv = smap_server_new(wordv[1], wordv[2], smap_session_server, 0);
	if (srv)
		smap_srvman_attach_server(srv);
	else
		return 1; /* FIXME: Error message */
	if (wordc == 4) {
		if (strcmp(wordv[3], "begin")) {
			smap_error("%s:%u: expected `begin' or end of line, "
				   "but found `%s'",
				   cfg_file_name, cfg_line, wordv[3]);
			return 1;
		}
		parse_config_loop(server_kwtab, srv);
	}
	return 0;
}

static int
cfg_append_load_path(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	if (cfg_chkargc(wordc, 2, 2))
		return 1;
	add_load_path(wordv[1], PATH_APPEND);
	return 0;
}

static int
cfg_prepend_load_path(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	if (cfg_chkargc(wordc, 2, 2))
		return 1;
	add_load_path(wordv[1], PATH_PREPEND);
	return 0;
}

static int
cfg_module(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	struct smap_module_instance *p;

	if (cfg_chkargc(wordc, 3, 0))
		return 1;
	if (module_declare(cfg_file_name, cfg_line,
			   wordv[1], wordc - 2, wordv + 2, &p)) {
		smap_error("%s:%u: module `%s' has already been declared",
			   cfg_file_name, cfg_line, wordv[1]);
		smap_error("%s:%u: this is the place of the previous declaration",
			   p->file, p->line);
		return 1;
	}
	return 0;
}

static int
cfg_database(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	struct smap_database_instance *p;

	if (cfg_chkargc(wordc, 3, 0))
		return 1;
	if (database_declare(cfg_file_name, cfg_line,
			     wordv[1], wordv[2], wordc - 2, wordv + 2, &p)) {
		smap_error("%s:%u: database `%s' has already been declared",
			   cfg_file_name, cfg_line, wordv[1]);
		smap_error("%s:%u: this is the place of the previous declaration",
			   p->file, p->line);
		return 1;
	}
	return 0;
}

static int
cfg_dispatch(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	if (cfg_chkargc(wordc, 3, 0))
		return 1;
	return parse_dispatch(wordv + 1);
}

static int
cfg_global_max_children(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	size_t n;

	if (cfg_chkargc(wordc, 2, 2))
		return 1;
	CFG_GETNUM(wordv[1], n);
	srvman_param.max_children = n;
	return 0;
}


static int
cfg_user(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	if (cfg_chkargc(wordc, 2, 2))
		return 1;

	privinfo_set_user(&global_privs, wordv[1]);
	return 0;
}

static int
cfg_group(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	int i;
	if (cfg_chkargc(wordc, 2, 0))
		return 1;

	for (i = 1; i < wordc; i++)
		privinfo_add_grpnam(&global_privs, wordv[i]);
	return 0;
}

static int
cfg_trace_pattern(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	int i;
	if (cfg_chkargc(wordc, 2, 0))
		return 1;

	for (i = 1; i < wordc; i++)
		add_trace_pattern(wordv[i]);
	return 0;
}

int
cfg_socket_mode(struct cfg_kw *kw, int wordc, char **wordv, void *data)
{
	if (cfg_chkargc(wordc, 2, 2))
		return 1;
	if (parse_mode_string(wordv[1], &srvman_param.socket_mode)) {
		smap_error("%s:%u: invalid file mode",
			   cfg_file_name, cfg_line);
		return 1;
	}
	return 0;
}

static struct cfg_kw smap_kwtab[] = {
	/*   kw        type       ival   sval  aval  fun */
	{ "inetd-mode", KWT_BOOL,  &inetd_mode },
	{ "pidfile", KWT_STRING, NULL, &pidfile, NULL, NULL },
	{ "foreground", KWT_BOOL, &foreground, },
	{ "idle-timeout", KWT_UINT, (int*) &idle_timeout },
	{ "log-to-stderr", KWT_BOOL, &log_to_stderr, },
	{ "log-to-syslog", KWT_BOOL, &log_to_stderr, NULL, NULL, bool_invert },
	{ "log-tag", KWT_STRING, NULL, &log_tag },
	{ "log-facility", KWT_FUN, &log_facility, NULL, NULL,
	  set_syslog_facility },
	{ "debug", KWT_FUN, NULL, NULL, NULL, cfg_debug },
	{ "trace", KWT_BOOL, &smap_trace_option },
	{ "trace-pattern", KWT_FUN, NULL, NULL, NULL, cfg_trace_pattern },

	/* Global privileges */
	{ "user",  KWT_FUN, NULL, NULL, NULL, cfg_user },
	{ "group", KWT_FUN, NULL, NULL, NULL, cfg_group },
	{ "allgroups", KWT_BOOL, &global_privs.allgroups },

	/* Srvman global parameters: */
	{ "socket-mode", KWT_FUN, NULL, NULL, NULL, cfg_socket_mode },
	{ "shutdown-timeout", KWT_UINT,
	  (int*) &srvman_param.shutdown_timeout },
	{ "backlog", KWT_INT, &srvman_param.backlog },
	{ "reuseaddr", KWT_BOOL, &srvman_param.reuseaddr },
	{ "max-children", KWT_FUN, NULL, NULL, NULL, cfg_global_max_children },
	{ "single-process", KWT_BOOL, &srvman_param.single_process },

	/* Server declarations */
	{ "server", KWT_FUN, NULL, NULL, NULL, cfg_server },

	/* Module Loader */
	{ "load-path", KWT_FUN, NULL, NULL, NULL, cfg_append_load_path },
	{ "append-load-path", KWT_FUN, NULL, NULL, NULL, cfg_append_load_path },
	{ "prepend-load-path", KWT_FUN, NULL, NULL, NULL, cfg_prepend_load_path },
	{ "module", KWT_FUN, NULL, NULL, NULL, cfg_module },
	{ "database", KWT_FUN, NULL, NULL, NULL, cfg_database },
	{ "dispatch", KWT_FUN, NULL, NULL, NULL, cfg_dispatch },
	{ NULL }
};


static void
debug_init()
{
	/* NOTE: Order is exactly the same as the of DBG_ values */
	static char *dbgname[] = { "smap",
				   "srvman",
				   "module",
				   "database",
				   "query",
				   "conf",
				   NULL };
	int i;
	for (i = 0; dbgname[i]; i++)
		smap_debug_alloc(dbgname[i]);
}

#include "cmdline.c"

int
main(int argc, char **argv)
{
	smap_set_program_name(argv[0]);
	debug_init();
	/* Logging is configured in two stages. Initial setup is based on
	   a guesswork: logs go to stderr if stdin is attached to a terminal.
	   After parsing configuration file and command line settings,
	   smap_log_init is called again to apply them. */
	if (!isatty(0))
		log_to_stderr = 0;
	smap_log_init();
	srvman_param.socket_mode = 0600;
	srvman_param.max_children = 128;
	/* Parse command line and configuration file. */
	get_options(argc, argv);
	if (parse_config(config_file, smap_kwtab, NULL))
		exit(EX_CONFIG);
	if (lint_mode)
		exit(0);

	/* Flush command line option settings */
	optcache_flush();

	/* Set up logging according to configuration settings. */
	if (log_to_stderr == -1)
		log_to_stderr = foreground;
	smap_log_init();

	/* Initialize module subsystem */
	smap_modules_init();
	smap_modules_load();
	init_databases();
	link_dispatch_rules();

	if (inetd_mode)
		smap_inet_server();
	else
		smap_daemon(argc, argv);

	free_databases();
	/*smap_modules_unload();*/

	exit(0);
}
