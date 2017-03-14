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
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <sysexits.h>
#ifdef HAVE_READLINE_READLINE_H
# include <readline/readline.h>
# include <readline/history.h>
#endif

#include "common.h"

#include <smap/wordsplit.h>
#include <smap/stream.h>
#include <smap/kwtab.h>
#include <smap/diag.h>
#include <smap/module.h>
#include <smap/url.h>

int smapc_trace_option;
int batch_mode;
struct in_addr source_addr;
#define DEFAULT_PROMPT "(smapc) "
char *prompt;
int cmdprefix = '.';
char *curmap;
char *curserver;
int norc;
int quiet_startup;
char *server_option;

#include "smapccmd.c"

int
open_socket(const char *url)
{
	int rc;
	int fd;
	struct sockaddr *sa;
	socklen_t salen;

	rc = smap_url_parse(url, &sa, &salen);
	if (rc) {
		smap_error("cannot parse url `%s': %s", url,
			   smap_url_strerror(rc));
		return -1;
	}
	fd = socket(sa->sa_family, SOCK_STREAM, 0);
	if (fd == -1) {
		smap_error("cannot create socket: %s", strerror(errno));
		return -1;
	}

	if (sa->sa_family == AF_INET) {
		struct sockaddr_in src;
		src.sin_family = AF_INET;
		src.sin_addr = source_addr;
		src.sin_port = 0;
		if (bind(fd, (struct sockaddr*) &src, sizeof(src)) < 0) {
			smap_error("cannot bind socket: %s", strerror(errno));
			return -1;
		}
	}

	if (connect(fd, sa, salen) < 0) {
		smap_error("cannot connect: %s", strerror(errno));
		return -1;
	}

	return fd;
}

char *replbuf;
size_t replsize;

static void
smapc_query(smap_stream_t str, const char *map, const char *key)
{
	int rc;
	size_t len;

	smap_stream_printf(str, "%s %s\n", map, key);
	rc = smap_stream_getline(str, &replbuf, &replsize, &len);
	if (rc) {
		smap_error("read error: %s",
			   smap_stream_strerror(str, rc));
		return;
	}
	replbuf[len] = 0;
	printf("%s", replbuf);
}

char *
get_input_line(FILE *fp, int interactive)
{
	char sbuf[512];
	static char *buf = NULL;
	static size_t bufsize = 0;
	size_t buflevel;

	if (interactive) {
#ifdef WITH_READLINE
		return readline(prompt);
#else
		fputs(prompt, stdout);
		fflush(stdout);
#endif
	}
	
	if (bufsize == 0) {
		bufsize = sizeof(sbuf);
		buf = emalloc(bufsize);
	}
	buflevel = 0;
	while (fgets(sbuf, sizeof sbuf, fp)) {
		size_t len = strlen(sbuf);
		if (buflevel + len >= bufsize) {
			bufsize *= 2;
			buf = erealloc(buf, bufsize);
		}
		strcpy(buf + buflevel, sbuf);
		buflevel += len;
		if (sbuf[len-1] == '\n')
			break;
	}
	if (buflevel == 0)
		return NULL;
	buf[buflevel-1] = 0;
	return buf;
}

static char *
user_file_name(const char *name, const char *suffix)
{
	static char *home;
	size_t size;
	char *filename;

	if (!home) {
		home = getenv("HOME");
		if (!home) {
			struct passwd *pw = getpwuid(getuid());
			if (!pw)
				return NULL;
			home = estrdup(pw->pw_dir);
		}
	}
	size = strlen(home) + 2 + strlen(name)
			    + (suffix ? strlen(suffix) : 0) + 1;
	filename = emalloc(size);
	strcpy(filename, home);
	strcat(filename, "/.");
	strcat(filename, name);
	if (suffix)
		strcat(filename, suffix);
	return filename;
}

#ifdef WITH_READLINE
#define HISTFILE_SUFFIX "_history"

static char *
get_history_file_name()
{
	static char *filename = NULL;

	if (!filename)
		filename = user_file_name (rl_readline_name, HISTFILE_SUFFIX);
	return filename;
}

static int
cmd_history(int argc, char **argv)
{
	HIST_ENTRY **hlist;
	int i;

	hlist = history_list();
	for (i = 0; i < history_length; i++)
		printf("%4d) %s\n", i + 1, hlist[i]->line);
	return 0;
}

#endif


static smap_stream_t iostr;

static void
smapc_close()
{
	if (iostr) {
		smap_stream_close(iostr);
		smap_stream_destroy(&iostr);
		free(curserver);
		curserver = NULL;
	}
}

static smap_stream_t
smapc_connect(const char *url)
{
	int fd;

	smapc_close();
	fd = open_socket(url);
	if (fd != -1) {
		int rc = smap_sockmap_stream_create(&iostr, fd, 0);
		if (rc) {
			smap_error("cannot create socket stream: %s",
				   strerror(rc));
			close(fd);
			return NULL;
		}
		curserver = estrdup(url);
		if (smap_debug_np(0, 10)) {
			char *pfx[] = { "S", "C" };
			int idx = 0;
			smap_stream_ioctl(iostr,
					  SMAP_IOCTL_SET_DEBUG_IDX,
					  &idx);
			smap_stream_ioctl(iostr,
					  SMAP_IOCTL_SET_DEBUG_PFX,
					  pfx);
		}
	}
	return iostr;
}


struct smapc_cmd {
	const char *name;
	int minargc;
	int maxargc;
	int (*fun)(int argc, char **argv);
	char *argdoc;
	char *docstring;
};

static int cmd_help(int argc, char **argv);

static int
cmd_quit(int argc, char **argv)
{
	return 1;
}

static int
cmd_map(int argc, char **argv)
{
	if (argc == 2) {
		free(curmap);
		curmap = estrdup(argv[1]);
	} else if (!curmap)
		printf("not set\n");
	else
		printf("current map is %s\n", curmap);
	return 0;
}

static int
cmd_nomap(int argc, char **argv)
{
	free(curmap);
	curmap = NULL;
	return 0;
}

static int
cmd_server(int argc, char **argv)
{
	if (!curserver)
		printf("server not set\n");
	else
		printf("current server is %s\n", curserver);
	return 0;
}

static int
cmd_open(int argc, char **argv)
{
	smapc_connect(argv[1]);
	return 0;
}

static int
cmd_close(int argc, char **argv)
{
	if (iostr)
		smapc_close();
	else
		smap_error("not open");
	return 0;
}

static int
cmd_source(int argc, char **argv)
{
	if (argc == 1)
		printf("current source IP is %s\n", inet_ntoa(source_addr));
	else if (inet_aton(argv[1], &source_addr) == 0)
	    smap_error("invalid IP address: %s", argv[1]);
	return 0;
}

static int
cmd_prefix(int argc, char **argv)
{
	if (argc == 1)
		printf("command prefix is %c\n", cmdprefix);
	else if (!(!argv[1][1] && argv[1][0] != '#' && ispunct(argv[1][0])))
		smap_error("expected single punctuation character");
	else {
#ifdef WITH_READLINE
		static char special_prefixes[2];
		strcpy(special_prefixes, argv[1]);
		rl_special_prefixes = special_prefixes;
#endif
		cmdprefix = argv[1][0];
	}
	return 0;
}

static int
cmd_prompt(int argc, char **argv)
{
	if (argc == 2) {
		free(prompt);
		prompt = estrdup(argv[1]);
	} else
		printf("current prompt is %s\n", prompt);
	return 0;
}

static int
cmd_quiet(int argc, char **argv)
{
	if (strcmp(argv[1], "yes") == 0
	    || strcmp(argv[1], "on") == 0
	    || strcmp(argv[1], "true") == 0
	    || strcmp(argv[1], "t") == 0)
		quiet_startup = 1;
	else if (strcmp(argv[1], "no") == 0
	    || strcmp(argv[1], "off") == 0
	    || strcmp(argv[1], "false") == 0
	    || strcmp(argv[1], "nil") == 0)
		quiet_startup = 0;
	else
		smap_error("invalid boolean value: %s", argv[1]);
	return 0;
}

static int
cmd_debug(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (smap_debug_set(argv[i]))
			smap_error("invalid debug specification: %s", argv[i]);
	}
	return 0;
}

static char gplv3_text[] = "\
   Smap is free software; you can redistribute it and/or modify\n\
   it under the terms of the GNU General Public License as published by\n\
   the Free Software Foundation; either version 3, or (at your option)\n\
   any later version.\n\
\n\
   Smap is distributed in the hope that it will be useful,\n\
   but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
   GNU General Public License for more details.\n\
\n\
   You should have received a copy of the GNU General Public License\n\
   along with Smap.  If not, see <http://www.gnu.org/licenses/>.\n";

static int
cmd_warranty(int argc, char **argv)
{
	print_version_only(program_version, stdout);
	putchar('\n');
	printf("%s", gplv3_text);
	return 0;
}

static int
cmd_version(int argc, char **argv)
{
	print_version_only(PACKAGE_STRING, stdout);
	return 0;
}
	

static struct smapc_cmd smapc_cmd_tab[] = {
	{ "help",   1, 1, cmd_help,   NULL, "show command summary" },
	{ "quit",   1, 1, cmd_quit,   NULL, "quit the shell" },
	{ "map",    1, 2, cmd_map,    "[NAME]", "set or query current map" },
	{ "nomap",  1, 1, cmd_nomap,  NULL, "clear current map" },
	{ "server", 1, 1, cmd_server, NULL, "show current server URL" },
	{ "open",   2, 2, cmd_open,   "URL", "open connection" },
	{ "close",  1, 1, cmd_close,  NULL, "close connection" },
	{ "source", 1, 2, cmd_source, "[IP]", "query or set source IP address" },
	{ "debug",  1, 0, cmd_debug,  "SPEC", "set debug level" },
	{ "prefix", 1, 2, cmd_prefix, "[CHAR]",
	  "set or query command prefix" },
	{ "prompt", 1, 2, cmd_prompt, "[STRING]",
	  "set or query command prompt" },
	{ "quiet",  2, 2, cmd_quiet,  "BOOL", "quiet startup" },
	{ "warranty", 1, 1, cmd_warranty, NULL, "print copyright statement" },
	{ "version", 1, 1, cmd_version, NULL, "print program version" },
#ifdef WITH_READLINE
	{ "history", 1, 1, cmd_history, NULL, "list input history" },
#endif
	{ NULL }
};

#ifdef WITH_READLINE

static char *
_command_generator(const char *text, int state)
{
	static int i, len;
	const char *name;

	if (!state) {
		i = 0;
		len = strlen (text);
		if (cmdprefix)
			len--;
	}
	if (cmdprefix)
		text++;
	while ((name = smapc_cmd_tab[i].name)) {
		i++;
		if (smapc_cmd_tab[i-1].docstring
		    && strncmp(name, text, len) == 0) {
			if (!cmdprefix)
				return strdup(name);
			else {
				char *p = emalloc(strlen(name) + 2);
				*p = cmdprefix;
				strcpy(p+1, name);
				return p;
			}
		}
	}

	return NULL;
}

static char **
_command_completion(const char *cmd, int start, int end)
{
	char **ret = NULL;

	if (!strrchr(rl_line_buffer, ' ')) {
		rl_completion_append_character = ' ';
		ret = rl_completion_matches (cmd, _command_generator);
		rl_attempted_completion_over = 1;
	}
	return ret;
}

static char *pre_input_line;

static int
pre_input (void)
{
	rl_insert_text(pre_input_line);
	free(pre_input_line);
	rl_pre_input_hook = NULL;
	rl_redisplay();
	return 0;
}

static int
retrieve_history(char *str)
{
	char *out;
	int rc;

	rc = history_expand(str, &out);
	switch (rc) {
	case -1:
		smap_error("%s", out);
		free(out);
		return 1;

	case 0:
		break;

	case 1:
		pre_input_line = out;
		rl_pre_input_hook = pre_input;
		return 1;

	case 2:
		printf("%s\n", out);
		free(out);
		return 1;
	}
	return 0;
}
#endif

static int
idxcmp(const void *a, const void *b)
{
	size_t ai = *(size_t*)a;
	size_t bi = *(size_t*)b;

	return strcmp(smapc_cmd_tab[ai].name, smapc_cmd_tab[bi].name);
}

static int
cmd_help(int argc, char **argv)
{
	static size_t *idx;
	static size_t nidx;
	size_t i;
	struct smapc_cmd *ft;

	if (!idx) {

		nidx = 0;
		for (ft = smapc_cmd_tab; ft->name; ft++)
			nidx++;
		idx = ecalloc(nidx, sizeof(idx[0]));
		for (i = 0; i < nidx; i++)
			idx[i] = i;

		qsort(idx, nidx, sizeof(idx[0]), idxcmp);
	}

	for (i = 0; i < nidx; i++) {
		int len = 0;
		const char *args;

		ft = smapc_cmd_tab + idx[i];
		if (ft->docstring == NULL)
			continue;
		if (cmdprefix) {
			printf("%c", cmdprefix);
			len++;
		}
		printf("%s ", ft->name);
		len += strlen(ft->name) + 1;
		if (ft->argdoc)
			args = ft->argdoc;
		else
			args = "";
		if (len < 24)
			len = 24 - len;
		else
			len = 0;
		printf("%-*s %s\n", len, args, ft->docstring);
	}
	return 0;
}


static struct smapc_cmd *
smapc_cmd_find(const char *word)
{
	struct smapc_cmd *p;

	for (p = smapc_cmd_tab; p->name; p++)
		if (strcmp(p->name, word) == 0)
			return p;
	return NULL;
}

int
handle_command(struct wordsplit *ws)
{
	struct smapc_cmd *cmd;

	cmd = smapc_cmd_find(ws->ws_wordv[0]);
	if (!cmd) {
		smap_error("unknown command");
		return 0;
	}
	if (cmd->minargc && ws->ws_wordc < cmd->minargc) {
		smap_error("too few arguments");
		return 0;
	}
	if (cmd->maxargc && ws->ws_wordc > cmd->maxargc) {
		smap_error("too many arguments");
		return 0;
	}
	return cmd->fun(ws->ws_wordc, ws->ws_wordv);
}

void
shell_banner()
{
	print_version("smapc (" PACKAGE_NAME ") " PACKAGE_VERSION, stdout);
	printf("%s\n\n", "Type ? for help summary");
}

static void
read_eval_loop(FILE *fp, int interactive)
{
	char *p;
	char *map;
	int flags = WRDSF_DEFFLAGS |
		    WRDSF_COMMENT |
		    WRDSF_ENOMEMABRT |
		    WRDSF_SHOWERR;
	struct wordsplit ws;

	ws.ws_comment = "#";

#ifdef WITH_READLINE
	if (interactive) {
		rl_readline_name = smap_progname;
		rl_attempted_completion_function = _command_completion;
		read_history(get_history_file_name());
	}
#endif
	if (interactive && !quiet_startup)
		shell_banner();
	while (p = get_input_line(fp, interactive)) {
#ifdef WITH_READLINE
		if (interactive) {
			if (retrieve_history(p))
				continue;
			add_history(p);
		}
#endif

		if (p[0] == '?') {
			cmd_help(0, NULL);
			continue;
		}
		if (cmdprefix == 0 || p[0] == cmdprefix) {
			if (wordsplit(p + !!cmdprefix, &ws, flags)) {
				wordsplit_perror(&ws);
				break;
			}
			flags |= WRDSF_REUSE;
			if (ws.ws_wordc && handle_command(&ws))
				break;
			continue;
		}

		while (*p && isspace(*p))
			p++;
		if (!*p || *p == '#')
			continue;

		if (!iostr) {
			smap_error("not connected, use %copen first",
				   cmdprefix);
			continue;
		}

		if (curmap)
			map = curmap;
		else {
			map = p;
			p = strchr(p, ' ');
			if (!p) {
				smap_error("invalid input");
				continue;
			}
			*p++ = 0;
			while (*p && isspace(*p))
				p++;
		}
		smapc_query(iostr, map, p);
	}
#ifdef WITH_READLINE
	if (interactive)
		write_history(get_history_file_name());
#endif
	if (flags & WRDSF_REUSE)
		wordsplit_free(&ws);
}

static void
smapc_config()
{
	char *fname = user_file_name(smap_progname, NULL);
	FILE *fp = fopen(fname, "r");
	int save_cmdprefix;

	if (!fp) {
		if (errno != ENOENT)
			smap_error("cannot open file %s", fname);
		free(fname);
		return;
	}
	save_cmdprefix = cmdprefix;
	cmdprefix = 0;
	read_eval_loop(fp, 0);
	cmdprefix = save_cmdprefix;
}

int
main(int argc, char **argv)
{
	int n;
	int interactive;
	
	smap_set_program_name(argv[0]);
	smap_openlog_stderr(smapc_trace_option);
	batch_mode = !isatty(0);
	smap_debug_alloc("smap");
	get_options(argc, argv, &n);
	argc -= n;
	argv += n;

	if (argc > 3) {
		smap_error("too many arguments");
		exit(EX_USAGE);
	}

	if (!norc)
		smapc_config();

	if (argc == 3) {
		/* FIXME if server_option */
		if (!smapc_connect(argv[0]))
			exit(EX_UNAVAILABLE); /* FIXME: or EX_USAGE */
		smapc_query(iostr, argv[1], argv[2]);
		exit(0);
	}

	if (!prompt)
		prompt = estrdup(DEFAULT_PROMPT);
	
	if (server_option)
		smapc_connect(server_option);

	if (argc) {
		curmap = estrdup(*argv++);
		argc--;
	}

	if (argc) {
		if (!iostr) {
			if (!curserver) {
				smap_error("server URL not given");
				exit(EX_USAGE);
			}
			exit(EX_UNAVAILABLE);
		}
		smapc_query(iostr, curmap, argv[0]);
		exit(0);
	}

	if (batch_mode) {
		interactive = 0;
		quiet_startup = 1;
	} else 
		interactive = isatty(0);

	read_eval_loop(stdin, interactive);

	smapc_close(iostr);

	exit(0);
}
