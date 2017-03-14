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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <syslog.h>
#include <grp.h>
#include <pwd.h>
#include <ctype.h>
#include <sysexits.h> /* FIXME */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ltdl.h>

#include <smap/wordsplit.h>
#include <smap/stream.h>
#include <smap/kwtab.h>
#include <smap/diag.h>
#include <smap/module.h>
#include <smap/url.h>

#include "common.h"
#include "srvman.h"

#if defined HAVE_SYSCONF && defined _SC_OPEN_MAX
# define getmaxfd() sysconf(_SC_OPEN_MAX)
#elif defined (HAVE_GETDTABLESIZE)
# define getmaxfd() getdtablesize()
#else
# define getmaxfd() 256
#endif

typedef union {
	struct sockaddr sa;
	struct sockaddr_in s_in;
	struct sockaddr_un s_un;
} all_addr_t;


/* A set of macros for declaring various typed doubly-linked lists.
   Will be removed when smap_list_t is implemented */
#define __mod_cat2__(a, b) a ## b
#define DCLATT(name, type, head, tail)	        \
static void				        \
__mod_cat2__(name,_attach)(struct type *arg)    \
{                                               \
	arg->next = NULL;                       \
	arg->prev = tail;                       \
	if (tail)                               \
		tail->next = arg;               \
	else	\
		head = arg;                     \
	tail = arg;                             \
}
#define DCLDET(name, type, head, tail)	        \
static void                                     \
__mod_cat2__(name,_detach)(struct type *arg)    \
{                                               \
	struct type *p;                         \
						\
	if ((p = arg->prev) != NULL)            \
		p->next = arg->next;            \
	else		\
		head = arg->next;               \
						\
	if ((p = arg->next) != NULL)            \
		p->prev = arg->prev;            \
	else				\
		tail = arg->prev;               \
}
#define DCLLOC(name, type, head)		\
struct type *                                   \
__mod_cat2__(name,_locate)(const char *id)      \
{                                               \
	struct type *p;                         \
						\
	for (p = head; p; p = p->next)          \
		if (strcmp(p->id, id) == 0)     \
			break;                  \
	return p;                               \
}
#define DCLLIST(name, type)		                                    \
	struct type *__mod_cat2__(name, _head), *__mod_cat2__(name, _tail); \
	DCLATT(name, type,                                                  \
	       __mod_cat2__(name, _head), __mod_cat2__(name, _tail))	    \
	DCLDET(name, type,                                                  \
	       __mod_cat2__(name, _head), __mod_cat2__(name, _tail))
#define DCLIDLIST(name, type)		                                    \
	DCLLIST(name, type)                                                 \
	DCLLOC(name, type, __mod_cat2__(name, _head))



/* log.c */
extern char *log_tag;
extern int log_facility;
extern int log_to_stderr;
extern int smap_trace_option;

void smap_log_init(void);
void add_trace_pattern(const char *arg);

#define DBG_SMAP 0
#define DBG_SRVMAN 1
#define DBG_MODULE 2
#define DBG_DATABASE 3
#define DBG_QUERY 4
#define DBG_CONF  5

#define debug smap_debug

int syslog_fname_to_n (const char *str, int *pint);

/* smap.c */
extern int foreground;
extern unsigned smap_timeout;
extern int inetd_mode;
extern int lint_mode;
extern char *pidfile;

/* cfg.c */
extern const char *cfg_file_name;
extern unsigned cfg_line;

enum kw_type {
	KWT_STRING,
	KWT_BOOL,
	KWT_INT,
	KWT_INT_O,
	KWT_INT_X,
	KWT_UINT,
	KWT_UINT_O,
	KWT_UINT_X,
	KWT_ARR,
	KWT_FUN,
	KWT_EOF
};

struct cfg_kw {
	const char *kw;
	enum kw_type type;
	int *ival;
	char **sval;
	char ***aval;
	int (*fun)(struct cfg_kw *, int, char **, void *);
};

int parse_config(const char *filename, struct cfg_kw *, void *);
void parse_config_loop(struct cfg_kw *kwtab, void *);

int cfg_chkargc(int wordc, int min, int max);
int cfg_parse_bool(const char *str, int *res);

/* module.c */
struct smap_module_instance {
	struct smap_module_instance *prev, *next;
	char *file;
	unsigned line;
	char *id;
	int argc;
	char **argv;
	struct smap_module *module;
	lt_dlhandle handle;
};

struct smap_database_instance {
	struct smap_database_instance *prev, *next;
	char *file;
	unsigned line;
	char *id;
	char *modname;
	int argc;
	char **argv;
	struct smap_module_instance *inst;
	smap_database_t dbh;
	int opened;
};

#define PATH_PREPEND 0
#define PATH_APPEND  1
void add_load_path(const char *path, int);
void smap_modules_init(void);
int module_declare(const char *file, unsigned line,
		   const char *id, int argc, char **argv,
		   struct smap_module_instance **pmod);
void smap_modules_load(void);
int database_declare(const char *file, unsigned line,
		     const char *id, const char *modname,
		     int argc, char **argv,
		     struct smap_database_instance **pdb);
struct smap_database_instance *database_locate(const char *id);

void init_databases(void);
void free_databases(void);
void close_databases(void);

/* query.c */
int parse_dispatch(char **wordv);
void link_dispatch_rules(void);
void dispatch_query(const char *id, struct smap_conninfo const *conninfo,
		    smap_stream_t ostr, const char *map, const char *key);

/* close-fds.c */
void close_fds_above(int fd);
void close_fds_except(fd_set *exfd);

/* userprivs.c */
struct privinfo {
	uid_t uid;
	gid_t *gids;
	size_t gc;
	size_t gi;
	int allgroups;
};

void get_user_groups(struct privinfo *pi);
int switch_to_privs(struct privinfo *pi);
void privinfo_set_user(struct privinfo *pi, const char *name);
void privinfo_add_grpgid(struct privinfo *pi, gid_t gid);
void privinfo_add_grpnam(struct privinfo *pi, const char *name);
void privinfo_expand_user_groups(struct privinfo *pi);
