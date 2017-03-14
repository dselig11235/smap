/* This file is part of Smap.
   Copyright (C) 2014 Sergey Poznyakoff

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
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <ldap.h>
#include <ctype.h>
#include <smap/stream.h>
#include <smap/diag.h>
#include <smap/module.h>
#include <smap/parseopt.h>
#include <smap/wordsplit.h>

static int dbgid;
static int ldap_debug_level; //FIXME

enum tls_state {
	tls_no,
	tls_yes,
	tls_only
};

/* module ldap ldap config-file=FILE uri=URI version=2|3
                    tls=no|yes|only
		    base=BASEDN
		    binddon=DN
		    bindpw=PASS
		    bindpwfile=FILE
                    filter=FILTER
		    positive-reply=EXPR
		    negative-reply=EXPR
		    onerror-reply=EXPR
 */

struct ldap_conf {
	enum tls_state tls;
	char *config_file;
	char *uri;
	char *base;
	long protocol;
	char *cacert;
	char *filter;
	char **attrs;
	
	char *binddn;
	char *bindpw;
	char *bindpwfile;
	
	char *positive_reply;
	char *negative_reply;
	char *onerror_reply;

	char *joinstr;
};

struct ldap_db {
	struct ldap_conf conf;
	LDAP *ldap;
};

static struct ldap_conf dfl_conf;
static char dfl_config_file[] = "/etc/ldap.conf";
static char dfl_positive_reply[] = "OK";
static char dfl_negative_reply[] = "NOTFOUND";
static char dfl_onerror_reply[] = "NOTFOUND";

static void
argz_free(char **a)
{
	int i;

	if (!a)
		return;
	for (i = 0; a[i]; i++)
		free(a[i]);
	free(a);
}

static int
argz_copy(char ***dst, char **a)
{
	int i, n;
	char **b;
	
	if (!a) {
		*dst = NULL;
		return 0;
	}
	
	for (n = 0; a[n]; n++)
		;

	b = calloc(i + 1, sizeof(b[0]));
	if (!b)
		return -1;
	for (i = 0; i < n; i++) {
		b[i] = strdup(a[i]);
		if (!b[i])
			return -1;
	}
	b[i] = NULL;
	return 0;
}

static void
ldap_conf_free(struct ldap_conf *cp)
{
	free(cp->config_file);
	free(cp->uri);
	free(cp->base);
	free(cp->cacert);
	free(cp->filter); 
	argz_free(cp->attrs);
	
	free(cp->binddn);
	free(cp->bindpw);
	free(cp->bindpwfile);
	
	free(cp->positive_reply);
	free(cp->negative_reply);
	free(cp->onerror_reply);

	free(cp->joinstr);
}

struct ldap_conf *
ldap_conf_cpy(struct ldap_conf *dst, struct ldap_conf *src)
{
#define STRCPY(a) do {				\
	if (src->a) {				\
		dst->a = strdup(src->a);	\
		if (!dst->a) {			\
			ldap_conf_free(dst);	\
			return NULL;		\
		}				\
	} else					\
		dst->a = src->a;		\
} while(0)
	

	memset(dst, 0, sizeof(*dst));
	dst->tls = src->tls;
        STRCPY(config_file);
	STRCPY(uri);
	STRCPY(base);
	dst->protocol = src->protocol;
	STRCPY(cacert);
	STRCPY(filter); 
        if (argz_copy(&dst->attrs, src->attrs)) {
		ldap_conf_free(dst);
		return NULL;
	}
		
	STRCPY(binddn);
	STRCPY(bindpw);
	STRCPY(bindpwfile);
	
	STRCPY(positive_reply);
	STRCPY(negative_reply);
	STRCPY(onerror_reply);

	STRCPY(joinstr);
	
        return dst;
}


static int
parse_ldap_conf(const char *name, struct smap_option const *opt)
{
	int rc = 0;
	FILE *fp;
	char buf[1024];
	unsigned line;
	char *p;
	
	fp = fopen(name, "r");
	if (!fp) {
		smap_error("can't open LDAP config file %s: %s",
			   name, strerror(errno));
		return -1;
	}

	line = 0;
	while (p = fgets(buf, sizeof(buf), fp)) {
		size_t len;
		char *errmsg;
		
		++line;
		
		while (*p && isspace(*p))
			++p;

		if (*p == '#')
			continue;
		
		len = strlen(p);
		if (len) {
			if (p[len-1] == '\n')
				p[--len] = 0;
			else if (!feof(fp)) {
				smap_error("%s:%u: line too long, skipping", name, line);
				while (!feof(fp) && fgetc(fp) != '\n')
					;
				++line;
				continue;
			}

			while (len > 0 && isspace(p[len-1]))
				--len;
		}
			
		if (len == 0)
			continue;
		p[len] = 0;

		switch (smap_parseline(opt, p, SMAP_IGNORECASE|SMAP_DELIM_WS,
				       &errmsg)) {
		case SMAP_PARSE_SUCCESS:
			smap_debug(dbgid, 2, ("%s:%u: accepted line %s",
					      name, line, p));
			break;	   
			
		case SMAP_PARSE_INVAL:
			smap_error("%s:%u: %s", name, line, errmsg);
			rc = 1;
			break;

		case SMAP_PARSE_NOENT:
			smap_debug(dbgid, 2, ("%s:%u: unrecognized line %s",
					      name, line, p));
		}
	}

	fclose(fp);
	return rc;
}

static int
readconf(struct smap_option const *opt, const char *val, char **errmsg)
{
	int rc = parse_ldap_conf(val, opt);
	if (rc)
		*errmsg = "parse error";
	return rc;
}

#define MKOPT_DEFAULT 0
#define MKOPT_REUSE 0x01
#define MKOPT_RESET 0x02

static int
make_options(struct ldap_conf *conf, int flags,
	     struct smap_option **ret_options)
{
	static struct smap_option init_option[] = {
		{ SMAP_OPTSTR(config-file), smap_opt_null,
		  (void*)offsetof(struct ldap_conf, config_file) },
		{ SMAP_OPTSTR(tls_cacert), smap_opt_string,
		  (void*)offsetof(struct ldap_conf, cacert) },
		{ SMAP_OPTSTR(tls-cacert), smap_opt_string,
		  (void*)offsetof(struct ldap_conf, cacert) },
		{ SMAP_OPTSTR(uri), smap_opt_string,
		  (void*)offsetof(struct ldap_conf, uri) },
		{ SMAP_OPTSTR(base), smap_opt_string,
		  (void*)offsetof(struct ldap_conf, base) },
		
		{ SMAP_OPTSTR(filter), smap_opt_string,
		  (void*)offsetof(struct ldap_conf, filter) },
		
		{ SMAP_OPTSTR(binddn), smap_opt_string,
		  (void*)offsetof(struct ldap_conf, binddn) },
	
		{ SMAP_OPTSTR(bindpw), smap_opt_string,
		  (void*)offsetof(struct ldap_conf, bindpw) },
		{ SMAP_OPTSTR(bindpwfile), smap_opt_string,
		  (void*)offsetof(struct ldap_conf, bindpwfile) },
                
                { SMAP_OPTSTR(ldap_version), smap_opt_long, 
                  (void*)offsetof(struct ldap_conf, protocol) },

		{ SMAP_OPTSTR(positive-reply), smap_opt_string,
		  (void*)offsetof(struct ldap_conf, positive_reply) },
		{ SMAP_OPTSTR(negative-reply), smap_opt_string,
		  (void*)offsetof(struct ldap_conf, negative_reply) },
		{ SMAP_OPTSTR(onerror-reply), smap_opt_string,
		  (void*)offsetof(struct ldap_conf, onerror_reply) },

		{ SMAP_OPTSTR(join-delim), smap_opt_string,
		  (void*)offsetof(struct ldap_conf, joinstr) },
		
		{ NULL }
	};
	int i;
	struct smap_option *opt;

	if (flags & MKOPT_REUSE)
		opt = *ret_options;
	else {
		opt = malloc(sizeof(init_option));
		if (!opt) 
			return 1;
		*ret_options = opt;
	}
	memcpy(opt, init_option, sizeof(init_option));
	
	if (flags & MKOPT_RESET)
		opt->type = smap_opt_string;
	for (i = 0; opt[i].name; i++) {
		opt[i].data = (char*)conf + (size_t) opt[i].data;
		if ((flags & MKOPT_RESET) && i)
			opt[i].type = smap_opt_null;
	}

	return 0;
}

static int
mod_ldap_init(int argc, char **argv)
{
	struct smap_option *opt;
	int rc;
	
	dbgid = smap_debug_alloc("ldap");

	if (make_options(&dfl_conf, MKOPT_DEFAULT, &opt)) {
		smap_error("not enough memory");
		return 1;
	}

	rc = smap_parseopt(opt, argc, argv, 0, NULL);
	free(opt);
	return rc;
}

static char *
argcv_concat(int wc, char **wv)
{
	char *res, *p;
	size_t size = 0;
	int i;

	for (i = 0; i < wc; i++)
		size += strlen(wv[i]) + 1;
	res = malloc(size);
	if (!res)
		return 0;
	for (p = res, i = 0;;) {
		strcpy(p, wv[i]);
		p += strlen(wv[i]);
		if (++i < wc)
			*p++ = ' ';
		else
			break;
	}
	*p = 0;
	return res;
}

char *
parse_ldap_uri(const char *uri)
{
	LDAPURLDesc *ludlist, **ludp;
	char **urls = NULL;
	int nurls = 0;
	char *ldapuri = NULL;
	int rc;
	
	rc = ldap_url_parse(uri, &ludlist);
	if (rc != LDAP_URL_SUCCESS) {
		smap_error("cannot parse LDAP URL(s)=%s (%d)",
			   uri, rc);
		return NULL;
	}
      
	for (ludp = &ludlist; *ludp; ) {
		LDAPURLDesc *lud = *ludp;
		char **tmp;
	  
		if (lud->lud_dn && lud->lud_dn[0]
		    && (lud->lud_host == NULL || lud->lud_host[0] == '\0'))  {
			/* if no host but a DN is provided, try
			   DNS SRV to gather the host list */
			char *domain = NULL, *hostlist = NULL;
			size_t i;
			struct wordsplit ws;
			
			if (ldap_dn2domain(lud->lud_dn, &domain) ||
			    !domain) {
				smap_error("DNS SRV: cannot convert "
					   "DN=\"%s\" into a domain",
					   lud->lud_dn);
				goto dnssrv_free;
			}
	      
			rc = ldap_domain2hostlist(domain, &hostlist);
			if (rc) {
				smap_error("DNS SRV: cannot convert "
					   "domain=%s into a hostlist",
					   domain);
				goto dnssrv_free;
			}

			if (wordsplit(hostlist, &ws, WRDSF_DEFFLAGS)) {
				smap_error("DNS SRV: could not parse hostlist=\"%s\": %s",
					   hostlist,
					   wordsplit_strerror(&ws));
				goto dnssrv_free;
			}
			
			tmp = realloc(urls, sizeof(char *) *
				      (nurls + ws.ws_wordc + 1));
			if (!tmp) {
				smap_error("DNS SRV %s", strerror(errno));
				goto dnssrv_free;
			}
			
			urls = tmp;
			urls[nurls] = NULL;
		
			for (i = 0; i < ws.ws_wordc; i++) {
				char *p = malloc(strlen(lud->lud_scheme) +
						 strlen(ws.ws_wordv[i]) +
						 3);
				if (!p) {
					smap_error("DNS SRV %s",
						   strerror(errno));
					goto dnssrv_free;
				}
			
				strcpy(p, lud->lud_scheme);
				strcat(p, "//");
				strcat(p, ws.ws_wordv[i]);
				
				urls[nurls + i + 1] = NULL;
				urls[nurls + i] = p;
			}
			
			nurls += i;
	      
		  dnssrv_free:
			wordsplit_free (&ws);
			ber_memfree(hostlist);
			ber_memfree(domain);
		} else {
			tmp = realloc(urls, sizeof(char *) * (nurls + 2));
			if (!tmp) {
				smap_error("DNS SRV %s", strerror(errno));
				break;
			}
			urls = tmp;
			urls[nurls + 1] = NULL;
			
			urls[nurls] = ldap_url_desc2str(lud);
			if (!urls[nurls]) {
				smap_error("DNS SRV %s",
					   strerror(errno));
				break;
			}
			nurls++;
		}
	
		*ludp = lud->lud_next;
		
		lud->lud_next = NULL;
		ldap_free_urldesc(lud);
	}

	if (ludlist) {
		ldap_free_urldesc(ludlist);
		return NULL;
	} else if (!urls)
		return NULL;
	ldapuri = argcv_concat(nurls, urls);
	if (!ldapuri)
		smap_error("%s", strerror(errno));
	ber_memvfree((void **)urls);
	return ldapuri;
}

static LDAP *
ldap_connect(struct ldap_conf *conf)
{
	int rc;
	char *ldapuri = NULL;
	LDAP *ld = NULL;
	char *val;
	unsigned long lval;

	if (ldap_debug_level) {
		if (ber_set_option(NULL, LBER_OPT_DEBUG_LEVEL,
				    &ldap_debug_level)
		    != LBER_OPT_SUCCESS )
			smap_error("cannot set LBER_OPT_DEBUG_LEVEL %d",
				   ldap_debug_level);

		if (ldap_set_option(NULL, LDAP_OPT_DEBUG_LEVEL,
				    &ldap_debug_level)
		    != LDAP_OPT_SUCCESS )
			smap_error("could not set LDAP_OPT_DEBUG_LEVEL %d",
				   ldap_debug_level);
	}

	if (conf->uri) {
		ldapuri = parse_ldap_uri(conf->uri);
		if (!ldapuri)
			return NULL;
	}

	smap_debug(dbgid, 1, ("constructed LDAP URI: %s",
			      ldapuri ? ldapuri : "<DEFAULT>"));

	rc = ldap_initialize(&ld, ldapuri);
	if (rc != LDAP_SUCCESS) {
		smap_error("cannot create LDAP session handle for "
			   "URI=%s (%d): %s",
			   ldapuri, rc, ldap_err2string(rc));
		free(ldapuri);
		return NULL;
	}
	free(ldapuri);

	if (conf->protocol) {
		int pn = (int) conf->protocol;
		ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &pn);
	}
	
	if (conf->tls != tls_no) {
		rc = ldap_start_tls_s(ld, NULL, NULL);
		if (rc != LDAP_SUCCESS) {
			char *msg = NULL;
			ldap_get_option(ld,
					LDAP_OPT_DIAGNOSTIC_MESSAGE,
					(void*)&msg);
			smap_error("ldap_start_tls failed: %s",
				   ldap_err2string(rc));
			smap_error("TLS diagnostics: %s", msg);
			ldap_memfree(msg);
			
			if (conf->tls == tls_only) {
				ldap_unbind_ext(ld, NULL, NULL);
				return NULL;
			}
			/* try to continue anyway */
		} else if (conf->cacert) {
			rc = ldap_set_option(ld,
					     LDAP_OPT_X_TLS_CACERTFILE,
					     conf->cacert);
			if (rc != LDAP_SUCCESS) {
				smap_error("setting of LDAP_OPT_X_TLS_CACERTFILE failed");
				if (conf->tls == tls_only) {
					ldap_unbind_ext(ld, NULL, NULL);
					return NULL;
				}
			}
		}
	}
	
	/* FIXME: Timeouts, SASL, etc. */
	return ld;
}

static int
full_read(int fd, char *file, char *buf, size_t size)
{
	while (size) {
		ssize_t n;

		n = read(fd, buf, size);
		if (n == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			smap_error("error reading from %s: %s",
				   file, strerror(errno));
                        return -1;
                } else if (n == 0) {
                        smap_error("short read from %s", file);
			return -1;
		}
		
		buf += n;
		size -= n;
	}
	return 0;
}

static int
get_passwd(struct ldap_conf *conf, struct berval *pwd, char **palloc)
{
	char *file;

	if (conf->bindpwfile) {
		struct stat st;
		int fd, rc;
		char *mem, *p;
		
		fd = open(file, O_RDONLY);
		if (fd == -1) {
			smap_error("can't open password file %s: %s",
				   file, strerror(errno));
			return -1;
		}
		if (fstat(fd, &st)) {
			smap_error("can't stat password file %s: %s",
				   file, strerror(errno));
			close(fd);
			return -1;
		}
		mem = malloc(st.st_size + 1);
		if (!mem) {
			smap_error("can't allocate memory (%lu bytes)",
				   (unsigned long) st.st_size+1);
			close(fd);
			return -1;
		}
		rc = full_read(fd, file, mem, st.st_size);
		close(fd);
		if (rc)
			return rc;
		mem[st.st_size] = 0;
		p = strchr(mem, '\n');
		if (p)
			*p = 0;
		*palloc = mem;
		pwd->bv_val = mem;
	} else
		pwd->bv_val = conf->bindpw;
	pwd->bv_len = pwd->bv_val ? strlen(pwd->bv_val) : 0;
	return 0;
}

static int
ldap_bind(LDAP *ld, struct ldap_conf *conf)
{
	int msgid, err, rc;
	LDAPMessage *result;
	LDAPControl **ctrls;
	char msgbuf[256];
	char *matched = NULL;
	char *info = NULL;
	char **refs = NULL;
	struct berval passwd;
	char *alloc_ptr = NULL;
	
	if (get_passwd(conf, &passwd, &alloc_ptr))
		return 1;

	msgbuf[0] = 0;
	
	rc = ldap_sasl_bind(ld, conf->binddn, LDAP_SASL_SIMPLE, &passwd,
			    NULL, NULL, &msgid);
	if (msgid == -1) {
		smap_error("ldap_sasl_bind(SIMPLE) failed: %s",
			   ldap_err2string(rc));
		free(alloc_ptr);
		return 1;
	}

	if (ldap_result(ld, msgid, LDAP_MSG_ALL, NULL, &result ) == -1) {
		smap_error("ldap_result failed");
		free(alloc_ptr);
		return 1;
	}

	rc = ldap_parse_result(ld, result, &err, &matched, &info, &refs,
			       &ctrls, 1);
	if (rc != LDAP_SUCCESS) {
		smap_error("ldap_parse_result failed: %s",
			   ldap_err2string(rc));
		free(alloc_ptr);
		return 1;
	}

	if (ctrls)
		ldap_controls_free(ctrls);
	
	if (err != LDAP_SUCCESS
	    || msgbuf[0]
	    || (matched && matched[0])
	    || (info && info[0])
	    || refs) {

		smap_debug(dbgid, 1, ("ldap_bind: %s (%d)%s",
				      ldap_err2string(err), err, msgbuf));

		if (matched && *matched) 
			smap_debug(dbgid, 1, ("matched DN: %s", matched));

		if (info && *info)
			smap_debug(dbgid, 1, ("additional info: %s", info));
		
		if (refs && *refs) {
			int i;
			smap_debug(dbgid, 3, ("referrals:"));
			for (i = 0; refs[i]; i++) 
				smap_debug(dbgid, 3, ("%s", refs[i]));
		}
	}

	if (matched)
		ber_memfree(matched);
	if (info)
		ber_memfree(info);
	if (refs)
		ber_memvfree((void **)refs);

	free(alloc_ptr);

	return !(err == LDAP_SUCCESS);
}


static smap_database_t
mod_ldap_init_db(const char *dbid, int argc, char **argv)
{
	LDAP *ldap;
	struct ldap_db *db;
	struct ldap_conf conf;
	size_t i, j;
	struct smap_option *opt;
	
	if (!ldap_conf_cpy(&conf, &dfl_conf))
		return NULL;

	if (make_options(&conf, MKOPT_RESET, &opt)) {
		smap_error("not enough memory");
		ldap_conf_free(&conf);
		return NULL;
	}

	if (smap_parseopt(opt, argc, argv, 0, NULL)) {
		ldap_conf_free(&conf);
		return NULL;
	}

	make_options(&conf, MKOPT_REUSE, &opt);
	
	if (!conf.config_file && access(dfl_config_file, R_OK) == 0)
		conf.config_file = strdup(dfl_config_file);

	if (conf.config_file && parse_ldap_conf(conf.config_file, opt)) {
		free(opt);
		ldap_conf_free(&conf);
	}

	if (smap_parseopt(opt, argc, argv, 0, NULL)) {
		free(opt);
		ldap_conf_free(&conf);
		return NULL;
	}
	free(opt);

	if (!conf.filter) {
		smap_error("%s: filter must be defined", dbid);
		ldap_conf_free(&conf);
		return NULL;
	}

	if (conf.positive_reply &&
	    wordsplit_varnames(conf.positive_reply, &conf.attrs, 0)) {
		smap_error("can't get attribute names: %s",
			   strerror(errno));
		ldap_conf_free(&conf);
		return NULL;
	}
	if (conf.negative_reply &&
	    wordsplit_varnames(conf.negative_reply, &conf.attrs, 1)) {
		smap_error("can't get attribute names: %s",
			   strerror(errno));
		ldap_conf_free(&conf);
		return NULL;
	}
	if (conf.onerror_reply &&
	    wordsplit_varnames(conf.onerror_reply, &conf.attrs, 1)) {
		smap_error("can't get attribute names: %s",
			   strerror(errno));
		ldap_conf_free(&conf);
		return NULL;
	}

	/* Eliminate map and key names */
	for (i = j = 0; conf.attrs[i]; i++, j++) {
		if (strcmp(conf.attrs[i], "map") == 0
		    || strcmp(conf.attrs[i], "key") == 0) {
			free(conf.attrs[i]);
			++j;
		}
		conf.attrs[i] = conf.attrs[j];
	}
	conf.attrs[i] = 0;
	
	db = calloc(1, sizeof(*db));
	if (!db) {
		ldap_conf_free(&conf);
		smap_error("%s: not enough memory", dbid);
		return NULL;
	}

	db->conf = conf;
	
	return (smap_database_t) db;
}

static int
mod_ldap_free_db(smap_database_t dbp)
{
	struct ldap_db *db = (struct ldap_db *) dbp;
	ldap_conf_free(&db->conf);
	free(db);
	return 0;
}

static int
mod_ldap_open(smap_database_t dbp)
{
	struct ldap_db *db = (struct ldap_db *) dbp;
	LDAP *ldap;

	ldap = ldap_connect(&db->conf);
	if (!ldap) 
		return 1;
	
	if (ldap_bind(ldap, &db->conf))
		return 1;

	db->ldap = ldap;
	
	return 0;
}

static int
mod_ldap_close(smap_database_t dbp)
{
	struct ldap_db *db = (struct ldap_db *) dbp;
	ldap_unbind_ext(db->ldap, NULL, NULL);
	db->ldap = NULL;
	return 0;
}

struct getvar_data {
	LDAP *ld;
	char const **env;
	LDAPMessage *msg;
	char *joinstr;
};

static char *
getvar(const char *var, size_t len, void *data)
{
	struct getvar_data *gd = data;
	
	if (len == 2 && memcmp(var, "dn", len) == 0)
		return ldap_get_dn(gd->ld, gd->msg);

	if (gd->env) {
		int i;

		for (i = 0; gd->env[i]; i += 2)
			if (strlen(gd->env[i]) == len
			    && memcmp(var, gd->env[i], len) == 0)
				return strdup(gd->env[i+1]);
	}

	if (gd->ld) {
		struct berval bv;
		char *p;
		struct berval **values;
		char *attr = malloc(len+1);

		if (!attr)
			return NULL;
		memcpy(attr, var, len);
		attr[len] = 0;
		values = ldap_get_values_len(gd->ld, gd->msg, attr);
		free(attr);
		if (!values)
			return strdup("");

		if (!gd->joinstr) {
			p = malloc(values[0]->bv_len + 1);
			if (!p)
				return NULL;
			memcpy(p, values[0]->bv_val, values[0]->bv_len);
			p[values[0]->bv_len] = 0;
		} else {
			size_t i, len, dlen = strlen(gd->joinstr);
			char *q;
			
			len = values[0]->bv_len;
			for (i = 1; values[i]; i++)
				len += values[i]->bv_len + dlen;
		        smap_debug(dbgid, 2, ("%s: %lu", var, (unsigned long)i));
			p = malloc(len + 1);
			if (!p)
				return NULL;
			q = p;
			for (i = 0; values[i]; i++) {
		                smap_debug(dbgid, 2,
                                           ("%s: %lu=%*.*s", var, 
					    (unsigned long)i,
					    (int)values[i]->bv_len,
					    (int)values[i]->bv_len,
					    values[i]->bv_val));
				if (i) {
					memcpy(q, gd->joinstr, dlen);
					q += dlen;
				}
				memcpy(q, values[i]->bv_val,
				       values[i]->bv_len);
				q += values[i]->bv_len;
			}
			*q = 0;
		}
		smap_debug(dbgid, 1, ("attr %*.*s=%s",
				      (int)len, (int)len, var, p));
		ldap_value_free_len(values);
		return p;
	}

	return NULL;
}

	
static int
send_reply(smap_stream_t ostr, const char *template, char const **env,
	   LDAPMessage *msg, struct ldap_db *db)
{
	struct wordsplit ws;
	int rc;
	struct getvar_data gd;
	
	ws.ws_env = (const char **) env;
	ws.ws_error = smap_error;
	ws.ws_getvar = getvar;
	if (db) {
		gd.ld = db->ldap;
		gd.joinstr = db->conf.joinstr;
	} else {
		gd.ld = NULL;
		gd.joinstr = NULL;
	}
	
	gd.env = env;
	gd.msg = msg;

	ws.ws_closure = &gd;
	rc = wordsplit(template, &ws,
		       WRDSF_NOSPLIT |
		       WRDSF_NOCMD |
		       WRDSF_GETVAR |
		       WRDSF_CLOSURE |
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
mod_ldap_query(smap_database_t dbp,
	       smap_stream_t ostr,
	       const char *map, const char *key,
	       struct smap_conninfo const *conninfo)
{
	struct ldap_db *db = (struct ldap_db *) dbp;
	char const *inenv[5];
	char **env;
	char **attrs;
	struct wordsplit ws;
	ber_int_t msgid;
	int rc;
	char *reply = NULL;
	LDAPMessage *res, *msg;

# define __smap_s_cat2__(a,b) a ## b
# define REPLY(d,s) \
	((d)->conf.__smap_s_cat2__(s,_reply)				\
	 ? (d)->conf.__smap_s_cat2__(s,_reply)				\
	 : __smap_s_cat3__(dfl_,s,_reply))

	
	inenv[0] = "map";
	inenv[1] = map;
	inenv[2] = "key";
	inenv[3] = key;
	inenv[4] = NULL;

	ws.ws_env = (const char **) inenv;
	ws.ws_error = smap_error;
	rc = wordsplit(db->conf.filter, &ws,
		       WRDSF_NOSPLIT |
		       WRDSF_NOCMD |
		       WRDSF_ENV |
		       WRDSF_ENV_KV |
		       WRDSF_ERROR |
		       WRDSF_SHOWERR);
	if (rc)
		return 1;

	smap_debug(dbgid, 2, ("using filter %s", ws.ws_wordv[0]));
	rc = ldap_search_ext(db->ldap, db->conf.base, LDAP_SCOPE_SUBTREE,
			     ws.ws_wordv[0], db->conf.attrs, 0,
			     NULL, NULL, NULL, -1, &msgid);

	if (rc != LDAP_SUCCESS)	{
		smap_error("ldap_search_ext: %s", ldap_err2string(rc));
		return send_reply(ostr, REPLY(db, onerror), inenv, NULL, NULL);
	}

	rc = ldap_result(db->ldap, msgid, LDAP_MSG_ALL, NULL, &res);
	if (rc < 0) {
		smap_error("ldap_result: %s", ldap_err2string(rc));
		return send_reply(ostr, REPLY(db, onerror), inenv, NULL, NULL);
	}

	msg = ldap_first_entry(db->ldap, res);
	if (!msg) {
		ldap_msgfree(res);
		return send_reply(ostr, REPLY(db, negative), inenv, NULL, NULL);
	}

	rc = send_reply(ostr, REPLY(db, positive), inenv, msg, db);
	ldap_msgfree(res);
	return rc;
}

struct smap_module SMAP_EXPORT(ldap, module) = {
	SMAP_MODULE_VERSION,
	SMAP_CAPA_DEFAULT,
	mod_ldap_init,
	mod_ldap_init_db,
	mod_ldap_free_db,
	mod_ldap_open,
	mod_ldap_close,
	mod_ldap_query,
	NULL, /* smap_xform */
};

