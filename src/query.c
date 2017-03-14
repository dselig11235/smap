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
#include <regex.h>
#include <fnmatch.h>

struct smap_sockaddr {
	unsigned netmask;
	int salen;
	struct sockaddr sa;
};

enum query_cond_type {
	query_cond_not,
	query_cond_source,
	query_cond_server,
	query_cond_map,
	query_cond_key
};

enum comparison {
	comp_eq,
	comp_like,
	comp_re
};

struct comp_cond {
	enum comparison op;
	char *str;
	regex_t re;
};

struct query_cond {
	struct query_cond *next;
	enum query_cond_type type;
	union {
		struct query_cond *subcond;       /* query_cond_not */
		struct smap_sockaddr *addr;       /* query_cond_source */
		char *id;                         /* query_cond_server */
		struct comp_cond comp;            /* query_cond_map/
						     query_cond_key */
	} v;
};

#define XFORM_NONE 0
#define XFORM_MAP  1
#define XFORM_KEY  2

struct dispatch_rule {
	struct dispatch_rule *prev, *next;
	char *file;
	unsigned line;
	struct query_cond *cond;
	char *dbname;
	struct smap_database_instance *dbi;
	int xform;
};


static struct query_cond *
query_cond_new(enum query_cond_type type)
{
	struct query_cond *p = ecalloc(1, sizeof(*p));
	p->type = type;
	return p;
}

static struct smap_sockaddr *
smap_sockaddr_new(int family, int len)
{
	struct smap_sockaddr *p = ecalloc(1, sizeof(*p));
	p->salen = len;
	p->sa.sa_family = family;
	return p;
}


#define S_UN_NAME(sa, salen) \
	((salen < offsetof (struct sockaddr_un,sun_path)) ? "" : (sa)->sun_path)

int
match_sockaddr(struct smap_sockaddr *sptr, struct sockaddr const *sa, int len)
{
	if (sptr->sa.sa_family != sa->sa_family)
		return 0;

	switch (sptr->sa.sa_family) {
	case AF_INET:
	{
		struct sockaddr_in *sin_clt = (struct sockaddr_in *)sa;
		struct sockaddr_in *sin_item = (struct sockaddr_in *)&sptr->sa;

		if (sin_item->sin_addr.s_addr ==
		    (ntohl (sin_clt->sin_addr.s_addr) & sptr->netmask))
			return 1;
		break;
	}

	case AF_UNIX:
	{
		struct sockaddr_un *sun_clt = (struct sockaddr_un *)sa;
		struct sockaddr_un *sun_item = (struct sockaddr_un *)&sptr->sa;

		if (S_UN_NAME (sun_clt, len)[0]
		    && S_UN_NAME (sun_item, sptr->salen)[0]
		    && strcmp (sun_clt->sun_path, sun_item->sun_path) == 0)
			return 1;
	}
	}
	return 0;
}


DCLLIST(dispatch, dispatch_rule);

enum query_tok {
	T_FROM,
	T_SERVER,
	T_MAP,
	T_LIKE,
	T_EQ,
	T_REGEXP,
	T_DB,
	T_NOT,
	T_TRANSFORM,
	T_KEY
};

static struct smap_kwtab query_kwtab[] = {
	{ "from", T_FROM },
	{ "server", T_SERVER },
	{ "map", T_MAP },
	{ "like", T_LIKE },
	{ "fnmatch", T_LIKE },
	{ "regexp", T_REGEXP },
	{ "eq", T_EQ },
	{ "is", T_EQ },
	{ "database", T_DB },
	{ "not", T_NOT },
	{ "transform", T_TRANSFORM },
	{ "key", T_KEY },
	{ NULL }
};

static char **input;

static char *
nextarg()
{
	if (!*input) {
		smap_error("%s:%u: unfinished query",
			   cfg_file_name, cfg_line);
		return NULL;
	}
	return *input++;
}


static int
parse_dispatch_server(struct query_cond **pcond)
{
	struct query_cond *cond;
	char *arg = nextarg();
	if (!arg)
		return 1;
	cond = query_cond_new(query_cond_server);
	cond->v.id = estrdup(arg);
	*pcond = cond;
	return 0;
}

static int parse_dispatch_not(struct query_cond **pcond);
static int parse_dispatch_comp(struct query_cond **pcond,
			       enum query_cond_type t);
static int parse_dispatch_from(struct query_cond **pcond);

static int
parse_subcond(struct query_cond **pcond)
{
	char *s;
	int tok;

	s = nextarg();
	if (!s)
		return 1;
	if (smap_kwtab_nametotok(query_kwtab, s, &tok)) {
		smap_error("%s:%u: unknown keyword: %s",
			   cfg_file_name, cfg_line, s);
		return 1;
	}
	switch (tok) {
	case T_NOT:
		return parse_dispatch_not(pcond);

	case T_FROM:
		return parse_dispatch_from(pcond);

	case T_SERVER:
		return parse_dispatch_server(pcond);

	case T_MAP:
		return parse_dispatch_comp(pcond, query_cond_map);

	case T_KEY:
		return parse_dispatch_comp(pcond, query_cond_key);
	}
	smap_error("%s:%u: unexpected keyword: %s",
		   cfg_file_name, cfg_line, s);
	smap_error("%s:%u: expected one of: not, from, to, map",
		   cfg_file_name, cfg_line);
	return 1;
};

static int
parse_dispatch_not(struct query_cond **pcond)
{
	struct query_cond *cond, *subcond;
	if (parse_subcond(&subcond))
		return 1;
	cond = query_cond_new(query_cond_not);
	cond->v.subcond = subcond;
	*pcond = cond;
	return 0;
}

static int
parse_regexp(const char *s, struct comp_cond *comp)
{
	int rc;
	char *buf;
	size_t len;
	int flags = REG_NOSUB | REG_EXTENDED;
	char *p;

	if (!ispunct(s[0])) {
		smap_error("%s:%u: regexp does not start with "
			   "a punctuation character: %s",
			   cfg_file_name, cfg_line, s);
		return 1;
	}

	p = strrchr(s, s[0]);
	if (p == s) {
		smap_error("%s:%u: unfinished regexp: %s",
			   cfg_file_name, cfg_line, s);
		return 1;
	}
	if (p[1]) {
		char *q;
		for (q = p + 1; *q; q++) {
			switch (*q) {
			case 'i':
				flags |= REG_ICASE;
				break;
			case 'b':
				flags &= ~REG_EXTENDED;
				break;
			case 'x':
				flags |= REG_EXTENDED;
				break;
			default:
				smap_error("%s:%u: uknown regexp "
					   "flag: %c",
					   cfg_file_name, cfg_line,
					   *q);
				return 1;
			}
		}
	}
	len = p - s - 1;
	buf = emalloc(len + 1);
	memcpy(buf, s + 1, len);
	buf[len] = 0;
	rc = regcomp(&comp->re, buf, flags);
	free(buf);
	if (rc) {
		char errbuf[512];
		regerror(rc, &comp->re, errbuf, sizeof(errbuf));
		smap_error("%s:%u: regexp error: %s",
			   cfg_file_name, cfg_line, errbuf);
		return 1;
	}
	comp->op = comp_re;
	return 0;
}

static int
parse_dispatch_comp(struct query_cond **pcond, enum query_cond_type t)
{
	struct query_cond *cond;
	struct comp_cond comp;
	char *s = nextarg();
	int tok;

	if (!s)
		return 1;

	if (smap_kwtab_nametotok(query_kwtab, s, &tok) == 0) {
		switch (tok) {
		case T_LIKE:
			s = nextarg();
			if (!s)
				return 1;
			comp.op = comp_like;
			break;

		case T_EQ:
			s = nextarg();
			if (!s)
				return 1;
			comp.op = comp_eq;
			break;

		case T_REGEXP:
			s = nextarg();
			if (!s)
				return 1;
			if (parse_regexp(s, &comp))
				return 1;
			break;

		default:
			comp.op = comp_eq;
		}
	} else
		comp.op = comp_eq;
	comp.str = estrdup(s);
	cond = query_cond_new(t);
	cond->v.comp = comp;
	*pcond = cond;
	return 0;
}

static int
parse_dispatch_from(struct query_cond **pcond)
{
	struct query_cond *cond;
	struct smap_sockaddr *sptr;
	char *string = nextarg();
	if (!string)
		return 1;

	if (string[0] == '/') {
		size_t len;
		struct sockaddr_un *s_un;

		len = strlen (string);
		if (len >= sizeof(s_un->sun_path)) {
			smap_error("%s:%u: socket name too long: `%s'",
				   cfg_file_name, cfg_line, string);
			return 1;
		}
		sptr = smap_sockaddr_new(AF_UNIX, sizeof(s_un));
		s_un = (struct sockaddr_un *) &sptr->sa;
		memcpy(s_un->sun_path, string, len);
		s_un->sun_path[len] = 0;
	} else {
		struct in_addr addr;
		struct sockaddr_in *s_in;
		char *p = strchr(string, '/');

		if (p)
			*p = 0;

		if (inet_aton(string, &addr) == 0) {
			struct hostent *hp = gethostbyname(string);
			if (!hp) {
				smap_error("%s:%u: cannot resolve host "
					   "name: `%s'",
					   cfg_file_name, cfg_line, string);
				if (p)
					*p = '/';
				return 1;
			}
			memcpy(&addr.s_addr, hp->h_addr, sizeof(addr.s_addr));
		}
		addr.s_addr = ntohl(addr.s_addr);

		sptr = smap_sockaddr_new(AF_INET, sizeof(s_in));
		s_in = (struct sockaddr_in *) &sptr->sa;
		s_in->sin_addr = addr;

		if (p) {
			*p++ = '/';
			char *q;
			unsigned netlen;

			netlen = strtoul(p, &q, 10);
			if (*q == 0) {
				if (netlen == 0)
					sptr->netmask = 0;
				else {
					sptr->netmask = 0xfffffffful >> (32 - netlen);
					sptr->netmask <<= (32 - netlen);
				}
			} else if (*q == '.') {
				struct in_addr addr;

				if (inet_aton(p, &addr) == 0) {
					smap_error("%s:%u: invalid "
						   "netmask: `%s'",
						   cfg_file_name, cfg_line, p);
					return 1;
				}
				sptr->netmask = addr.s_addr;
			} else {
				smap_error("%s:%u: invalid netmask: `%s'",
					   cfg_file_name, cfg_line, p);
				return 1;
			}
		} else
			sptr->netmask = 0xfffffffful;
	}
	cond = query_cond_new(query_cond_source);
	cond->v.addr = sptr;
	*pcond = cond;
	return 0;
}

int
parse_complex_dispatch()
{
	int rc = 0;
	struct query_cond *head = NULL, *tail = NULL;
	char *dbname = NULL;
	struct dispatch_rule *rp;
	int xform = XFORM_NONE;
	
	while (*input && rc == 0) {
		char *s = *input++;
		int tok, subtok;
		struct query_cond *cond = NULL;

		if (smap_kwtab_nametotok(query_kwtab, s, &tok)) {
			smap_error("%s:%u: unknown keyword: %s",
				   cfg_file_name, cfg_line, s);
			rc = 1;
			break;
		}

		switch (tok) {
		case T_NOT:
			rc = parse_dispatch_not(&cond);
			break;

		case T_FROM:
			rc = parse_dispatch_from(&cond);
			break;

		case T_SERVER:
			rc = parse_dispatch_server(&cond);
			break;

		case T_MAP:
			rc = parse_dispatch_comp(&cond, query_cond_map);
			break;

		case T_KEY:
			rc = parse_dispatch_comp(&cond, query_cond_key);
			break;

		case T_TRANSFORM:
			s = nextarg();
			if (!s) {
				rc = 1;
				break;
			}
			if (smap_kwtab_nametotok(query_kwtab, s, &subtok)) {
				smap_error("%s:%u: unknown keyword: %s",
					   cfg_file_name, cfg_line, s);
				rc = 1;
				break;
			}
			
		case T_DB:
			s = nextarg();
			if (!s)
				rc = 1;
			else if (dbname) {
				smap_error("%s:%u: database specified twice",
					   cfg_file_name, cfg_line);
				rc = 1;
			} else
				dbname = estrdup(s);
			if (tok == T_TRANSFORM) {
				if (subtok == T_MAP)
					xform = XFORM_MAP;
				else if (subtok == T_KEY)
					xform = XFORM_KEY;
				else {
					smap_error("%s:%u: unexpected keyword"
						   "; expected map or key",
						   cfg_file_name, cfg_line);
					rc = 1;
				}
			}
		}
		if (rc == 1)
			break;
		if (cond) {
			if (tail)
				tail->next = cond;
			else
				head = cond;
			tail = cond;
		}
	}

	if (!dbname) {
		smap_error("%s:%u: database not specified",
			   cfg_file_name, cfg_line);
		/* FIXME: Free collected data */
		return 1;
	}
	rp = ecalloc(1, sizeof(*rp));
	rp->file = estrdup(cfg_file_name);
	rp->line = cfg_line;
	rp->cond = head;
	rp->dbname = dbname;
	rp->xform = xform;
	dispatch_attach(rp);
	return 0;
}

int
parse_default_dispatch()
{
	struct dispatch_rule *rp;
	int tok;
	char *s = nextarg();
	if (!s)
		return 1;
	if (smap_kwtab_nametotok(query_kwtab, s, &tok) || tok != T_DB) {
		smap_error("%s:%u: expected `database' but found `%s'",
			   cfg_file_name, cfg_line, s);
		return 1;
	}
	s = nextarg();
	if (!s)
		return 1;
	if (*input) {
		smap_error("%s:%u: garbage after database name",
			   cfg_file_name, cfg_line);
		return 1;
	}
	rp = ecalloc(1, sizeof(*rp));
	rp->file = estrdup(cfg_file_name);
	rp->line = cfg_line;
	rp->cond = NULL;
	rp->dbname = estrdup(s);
	dispatch_attach(rp);
	return 0;
}

int
parse_dispatch(char **wordv)
{
	input = wordv;

	if (*input && strcmp(*input, "default") == 0) {
		input++;
		return parse_default_dispatch();
	}
	return parse_complex_dispatch();
}

int
rule_fixup(struct dispatch_rule *p)
{
	struct smap_database_instance *dbi;
	struct smap_module *mod;

	dbi = database_locate(p->dbname);
	if (!dbi) {
		smap_error("%s:%u: no such database: %s",
			   p->file, p->line, p->dbname);
		return 1;
	}

	mod = dbi->inst->module;
	if (mod->smap_version == 1) {
		if (p->xform) {
			smap_error("%s:%u: database %s does not "
					   "handle transformations",
					   p->file, p->line, p->dbname);
			return 1;
		}
	} else if (mod->smap_version > 1) {
		if (p->xform) {
			if (!(mod->smap_capabilities & SMAP_CAPA_XFORM)) {
				smap_error("%s:%u: database %s does not "
					   "handle transformations",
					   p->file, p->line, p->dbname);
				return 1;
			}
		} else {
			if (!(mod->smap_capabilities & SMAP_CAPA_QUERY)) {
				smap_error("%s:%u: database %s does not "
					   "handle queries: %x",
					   p->file, p->line, p->dbname,
					mod->smap_capabilities);
				return 1;
			}
		}
	}
	p->dbi = dbi;
	return 0;
}

void
link_dispatch_rules()
{
	struct dispatch_rule *p;

	for (p = dispatch_head; p; ) {
		struct dispatch_rule *next = p->next;
		if (rule_fixup(p)) {
			debug(DBG_MODULE, 1, ("removing query %s:%u",
					      p->file, p->line));
			dispatch_detach(p);
			/* FIXME: free memory */
		}
		p = next;
	}
}

struct query_pack {
	const char *server_id;
	struct smap_conninfo const *conninfo;
	const char *map;
	const char *key;
	char *storage[2];
};

static int
compare(struct comp_cond *cond, const char *map)
{
	switch (cond->op) {
	case comp_eq:
		return strcmp(cond->str, map) == 0;

	case comp_like:
		return fnmatch(cond->str, map, 0) == 0;

	case comp_re:
		return regexec(&cond->re, map, 0, NULL, 0) == 0;
	}
	return 0;
}

static int
match_cond(struct query_cond *cond, struct query_pack *qp)
{
	switch (cond->type) {
	case query_cond_not:
		return match_cond(cond->v.subcond, qp);

	case query_cond_source:
		return match_sockaddr(cond->v.addr, qp->conninfo->src,
				      qp->conninfo->srclen);

	case query_cond_server:
		/* FIXME: Implement like/regex */
		return strcmp(cond->v.id, qp->server_id) == 0;

	case query_cond_map:
		return compare(&cond->v.comp, qp->map);
		
	case query_cond_key:
		return compare(&cond->v.comp, qp->key);
	}
	return 0;
}

static int
match_cond_list(struct query_cond *cond, struct query_pack *qp)
{
	for (; cond; cond = cond->next)
		if (!match_cond(cond, qp))
			return 0;
	return 1;
}

static struct dispatch_rule *
find_dispatch_rule(struct query_pack *qp, struct dispatch_rule *start)
{
	struct dispatch_rule *p;

	if (!start)
		start = dispatch_head;
	for (p = start; p; p = p->next) {
		debug(DBG_QUERY, 2, ("trying %s:%u", p->file, p->line));
		if (match_cond_list(p->cond, qp))
			break;
	}
	return p;
}

static void
dispatch_query_pack(struct query_pack *qp,
		    struct smap_conninfo const *conninfo, smap_stream_t ostr)
{
	struct dispatch_rule *next = NULL;
	struct smap_database_instance *dbi;
	struct smap_module *mod;

	debug(DBG_QUERY, 1, ("dispatching query %s %s", qp->map, qp->key));
	do {
		struct dispatch_rule *qr;
		
		qr = find_dispatch_rule(qp, next);
		if (qr)
			debug(DBG_QUERY, 1, ("rule at %s:%u, database %s",
					     qr->file, qr->line, qr->dbname));
		else
			break;

		dbi = qr->dbi;
		mod = dbi->inst->module;
		if (!dbi->opened) {
			int rc = 0;
			
			debug(DBG_DATABASE, 2,
			      ("opening database %s", dbi->id));
			if (mod->smap_open)
				rc = mod->smap_open(dbi->dbh);
			if (rc) {
				smap_error("cannot open database %s", dbi->id);
				/* FIXME: mark database unusable */
				smap_stream_printf(ostr, "NOTFOUND\n");
				return;
			}
			dbi->opened = 1;
		}

		if (qr->xform) {
			const char **parg;
			char *narg = NULL;
			static const char *what[]= { "map", "key" };
	
			if (qr->xform == XFORM_KEY)
				parg = &qp->key;
			else
				parg = &qp->map;
			if (mod->smap_xform(dbi->dbh,
					    conninfo, *parg, &narg) == 0) {
				if (narg) {
					debug(DBG_QUERY, 1,
					      ("rule at %s:%u, transformed %s: %s => %s",
					       qr->file, qr->line, what[qr->xform - 1],
					       *parg, narg));
					if (qp->storage[qr->xform-1])
						free(qp->storage[qr->xform-1]);
					qp->storage[qr->xform-1] = narg;
					*parg = narg;
				}
			} else
				smap_error("%s:%u: transformation failed",
					   qr->file, qr->line);
			next = qr->next;
		} else {		 
			if (mod->smap_query(dbi->dbh, ostr,
					    qp->map, qp->key,
					    conninfo))
				break;
			return;
		}
	} while (next);
	smap_error("no database matches %s %s", qp->map, qp->key);
	smap_stream_printf(ostr, "NOTFOUND\n");
}

void
dispatch_query(const char *id, struct smap_conninfo const *conninfo,
	       smap_stream_t ostr, const char *map, const char *key)
{
	struct query_pack query;
	
	query.server_id = id;
	query.conninfo = conninfo;
	query.map = map;
	query.key = key;
	query.storage[0] = query.storage[1] = NULL;
	dispatch_query_pack(&query, conninfo, ostr);
	free(query.storage[0]);
	free(query.storage[1]);
}
