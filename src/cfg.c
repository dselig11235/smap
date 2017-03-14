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

FILE *infile;
const char *cfg_file_name;
unsigned cfg_line;
unsigned cfg_cur_line;
int cfg_errors;

#define INITBUFSIZE 512
#define MINBUFSIZE  8

int
read_line(char **pbuf, size_t *psize)
{
	char *buf = *pbuf;
	size_t size = *psize;
	size_t off = 0;

	if (feof(infile))
		return 0;
	cfg_line = cfg_cur_line;
	if (size == 0) {
		size = INITBUFSIZE;
		buf = emalloc(size);
	}

	while (1) {
		size_t len = size - off;

		if (len < MINBUFSIZE) {
			size *= 2;
			buf = realloc(buf, size);
		}

		if (fgets(buf + off, size - off, infile) == NULL)
			break;

		len = strlen(buf);

		if (len == 0)
			continue;
		if (buf[len-1] != '\n') {
			off = len;
			continue;
		}
		buf[--len] = 0;
		cfg_cur_line++;
		if (len == 0)
			continue;
		if (buf[len-1] == '\\') {
			off = len - 1;
			continue;
		}
		off = len;
		break;
	}
	*pbuf = buf;
	*psize = size;
	return off;
}

static struct cfg_kw *
find_cfg_kw(struct cfg_kw *kwtab, const char *word)
{
	for (; kwtab->kw; kwtab++)
		if (strcmp(kwtab->kw, word) == 0)
			return kwtab;
	return NULL;
}

int
cfg_chkargc(int wordc, int min, int max)
{
	if (wordc < min) {
		smap_error(min == 2 ?
			     "%s:%u: required argument missing" :
			     "%s:%u: required arguments missing",
			   cfg_file_name, cfg_line);
		cfg_errors = 1;
		return 1;
	} else if (max > 0 && wordc > max) {
		smap_error("%s:%u: too many arguments",
			   cfg_file_name, cfg_line);
		cfg_errors = 1;
		return 1;
	}
	return 0;
}

int
cfg_parse_bool(const char *str, int *res)
{
	if (strcmp (str, "yes") == 0
	    || strcmp (str, "on") == 0
	    || strcmp (str, "t") == 0
	    || strcmp (str, "true") == 0
	    || strcmp (str, "1") == 0)
		*res = 1;
	else if (strcmp (str, "no") == 0
		 || strcmp (str, "off") == 0
		 || strcmp (str, "nil") == 0
		 || strcmp (str, "false") == 0
		 || strcmp (str, "0") == 0)
		*res = 0;
	else {
		smap_error("%s:%u: unrecognized boolean value (%s)",
			   cfg_file_name, cfg_line, str);
		cfg_errors = 1;
		return 1;
	}
	return 0;
}

static void
parse_int(const char *str, int base, int sign, int *res)
{
	char *p;
	unsigned long n = strtoul(str, &p, base);

	if (*p) {
		smap_error("%s:%u: invalid numeric value (near %s)",
			   cfg_file_name, cfg_line, p);
		cfg_errors = 1;
		return;
	}

	if (sign) {
		int i;

		if ((i = n) == n) {
			*res = i;
			return;
		}
	} else {
		unsigned u;

		if ((u = n) == n) {
			*res = u;
			return;
		}
	}
	smap_error("%s:%u: numeric value out of range",
		   cfg_file_name, cfg_line);
	cfg_errors = 1;
}

static void
save_words(struct wordsplit *ws, struct cfg_kw *kwp)
{
	int i, n;
	char **aval;

	aval = ecalloc(ws->ws_wordc, sizeof(kwp->aval[0]));
	*kwp->ival = n = ws->ws_wordc - 1;
	for (i = 0; i < n; i++)
		aval[i] = estrdup(ws->ws_wordv[i + 1]);
	aval[i] = 0;
	*kwp->aval = aval;
}

int
wrdse_to_ex(int code)
{
  	switch (code) {
  	case WRDSE_EOF:
  		return 0;
  	case WRDSE_QUOTE:
  		return EX_CONFIG;
  	case WRDSE_NOSPACE:
  		return EX_UNAVAILABLE;
  	case WRDSE_NOSUPP:
  		return EX_SOFTWARE;
	}
	return EX_UNAVAILABLE;
}

static int
asgn_p(char *buf)
{
	unsigned char *p = (unsigned char*)buf;
	while (*p && (*p == ' ' || *p == '\t'))
		p++;
	if (!*p)
		return 0;
	if (*p < 127 && (isalpha(*p) || *p == '_')) {
		while (*p && *p < 127 && (isalnum(*p) || *p == '_'))
			p++;
		return *p == '=';
	}
	return 0;
}

static size_t
find_env_var(const char **env, char *name)
{
	size_t i;
		
	for (i = 0; env[i]; i++) {
		size_t j;
		const char *var = env[i];
		
		for (j = 0; name[j]; j++) {
			if (name[j] != var[j])
				break;
			if (name[j] == '=')
				return i;
		}
	}
	return i;
}

static void
asgn(struct wordsplit *ws, size_t *psize)
{
	size_t size = *psize;

	if (size == 0) {
		size = 16;
		ws->ws_env = ecalloc(size, sizeof(ws->ws_env[0]));
		ws->ws_env[0] = ws->ws_wordv[0];
	} else {
		size_t n = find_env_var(ws->ws_env, ws->ws_wordv[0]);
		if (ws->ws_env[n])
			free((char*)ws->ws_env[n]);
		else if (n == size - 1) {
			size *= 2;
			ws->ws_env = erealloc(ws->ws_env,
					      size * sizeof(ws->ws_env[0]));
		}
		ws->ws_env[n] = ws->ws_wordv[0];
		ws->ws_env[++n] = NULL;
	}
	ws->ws_wordv[0] = NULL;
	*psize = size;
}


static void
smapd_ws_debug(const char *fmt, ...)
{
	va_list ap;
	
	smap_stream_printf(smap_debug_str, "WS: ");
	va_start(ap, fmt);
	smap_stream_vprintf(smap_debug_str, fmt, ap);
	va_end(ap);
	smap_stream_write(smap_debug_str, "\n", 1, NULL);
}

void
parse_config_loop(struct cfg_kw *kwtab, void *data)
{
	char *buf = NULL;
	size_t size = 0;
	struct wordsplit ws;
	int wsflags = WRDSF_DEFFLAGS |
		      WRDSF_ERROR |
		      WRDSF_COMMENT |
		      WRDSF_ENOMEMABRT;
	size_t envsize = 0;
	int eof = 0;

	ws.ws_error = smap_error;
	ws.ws_comment = "#";

	if (smap_debug_np(DBG_CONF, 1))
		wsflags |= WRDSF_WARNUNDEF;
	if (smap_debug_np(DBG_CONF, 100)) {
		ws.ws_debug = smapd_ws_debug;
		wsflags |= WRDSF_DEBUG | WRDSF_SHOWDBG;
	}
	
	while (!eof && read_line(&buf, &size)) {
		struct cfg_kw *kwp;

		if (wordsplit(buf, &ws, wsflags)) {
			smap_error("%s:%u: %s",
				   cfg_file_name, cfg_line, 
				   wordsplit_strerror(&ws));
			exit(wrdse_to_ex(ws.ws_errno));
		}
		wsflags |= WRDSF_REUSE;

		if (ws.ws_wordc == 0)
			continue;

		if (smap_debug_np(DBG_CONF, 2)) {
			size_t i;
			
			smap_stream_printf(smap_debug_str,
					   "%s:%u: [%lu args]:",
					   cfg_file_name, cfg_line,
					   (unsigned long)ws.ws_wordc);
			for (i = 0; i < ws.ws_wordc; i++)
				smap_stream_printf(smap_debug_str, " '%s'",
						   ws.ws_wordv[i]);
			smap_stream_printf(smap_debug_str, "\n");
		}
		
		if (asgn_p(ws.ws_wordv[0])) {
			if (ws.ws_wordc > 1) {
				smap_error("%s:%u: too many arguments "
					   "(unquoted assignment?)",
					   cfg_file_name, cfg_line);
				cfg_errors = 1;
			} else {
				asgn(&ws, &envsize);
				wsflags &= ~WRDSF_NOVAR;
				wsflags |= WRDSF_ENV | WRDSF_KEEPUNDEF;
			}
			continue;
		}
		
		kwp = find_cfg_kw(kwtab, ws.ws_wordv[0]);
		if (!kwp) {
			smap_error("%s:%u: unrecognized line",
				   cfg_file_name, cfg_line);
			cfg_errors = 1;
			continue;
		}
		switch (kwp->type) {
		case KWT_STRING:
			if (cfg_chkargc(ws.ws_wordc, 2, 2))
				continue;
			*kwp->sval = estrdup(ws.ws_wordv[1]);
			break;

		case KWT_BOOL:
			if (cfg_chkargc(ws.ws_wordc, 2, 2))
				continue;
			cfg_parse_bool(ws.ws_wordv[1], kwp->ival);
			break;

		case KWT_INT:
		case KWT_UINT:
			if (cfg_chkargc(ws.ws_wordc, 2, 2))
				continue;
			parse_int(ws.ws_wordv[1], 10,
				  kwp->type == KWT_INT,
				  kwp->ival);
			break;

		case KWT_INT_O:
		case KWT_UINT_O:
			if (cfg_chkargc(ws.ws_wordc, 2, 2))
				continue;
			parse_int(ws.ws_wordv[1], 8,
				  kwp->type == KWT_INT_O,
				  kwp->ival);
			break;

		case KWT_INT_X:
		case KWT_UINT_X:
			if (cfg_chkargc(ws.ws_wordc, 2, 2))
				continue;
			parse_int(ws.ws_wordv[1], 16,
				  kwp->type == KWT_INT_X,
				  kwp->ival);
			break;

		case KWT_ARR:
			if (cfg_chkargc(ws.ws_wordc, 2, 0))
				continue;
			save_words(&ws, kwp);
			break;

		case KWT_FUN:
			break;

		case KWT_EOF:
			eof = 1;
			break;

		default:
			smap_error("INTERNAL ERROR at %s:%d, please report",
				   __FILE__, __LINE__);
			abort();
		}
		if (kwp->fun) {
			int rc = kwp->fun(kwp, ws.ws_wordc, ws.ws_wordv, data);
			if (eof) {
				if (rc < 0) {
					cfg_errors = 1;
					break;
				}
				if (rc == 0)
					eof = 0;
			} else if (rc)
				cfg_errors = 1;
		}
	}
	if (wsflags & WRDSF_REUSE)
		wordsplit_free(&ws);
	if (envsize)
		free(ws.ws_env);
	free(buf);
}

int
parse_config(const char *filename, struct cfg_kw *kwtab, void *data)
{
	infile = fopen(filename, "r");
	if (!infile) {
		smap_error("cannot open file %s: %s", filename,
			   strerror(errno));
		return -1;
	}
	cfg_file_name = filename;
	cfg_cur_line = 1;
	parse_config_loop(kwtab, data);
	fclose(infile);
	return cfg_errors;
}
