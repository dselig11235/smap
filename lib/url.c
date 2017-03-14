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
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>

#include <smap/url.h>

static char *url_error_string[] = {
	"no error",
	"not enough memory",                     /* SMAP_URLE_NOMEM */
	"malformed URL",                         /* SMAP_URLE_INVALID */
	"port is meaningless for UNIX sockets",  /* SMAP_URLE_PORT */
	"UNIX socket name too long",             /* SMAP_URLE_NAME2LONG */
	"UNIX socket name must be absolute",     /* SMAP_URLE_NOABS */
	"missing port number",                   /* SMAP_URLE_NOPORT */
	"bad port number",                       /* SMAP_URLE_BADPORT */
	"unknown port name",                     /* SMAP_URLE_UNKPORT */
	"unknown host name",                     /* SMAP_URLE_UNKHOST */
	"unsupported address family",            /* SMAP_URLE_BADFAMILY */
	"unsupported protocol",                  /* SMAP_URLE_BADPROTO */
};

const char *
smap_url_strerror(int er)
{
	if (er < 0
	    || er >= sizeof(url_error_string)/sizeof(url_error_string[0]))
		return "unrecognized error";
	return url_error_string[er];
}

static int
copy_part(const char *cstr, const char *p, char **pbuf)
{
	size_t len = p - cstr;
	char *buf = malloc(len + 1);
	if (!buf) {
		free(buf);
		return SMAP_URLE_NOMEM;
	}
	memcpy(buf, cstr, len);
	buf[len] = 0;
	*pbuf = buf;
	return 0;
}


static int
split_url(const char *proto, const char *cstr, char **pport, char **ppath)
{
	const char *p = strchr(cstr, ':');

	if (!p) {
		*pport = NULL;
		*ppath = strdup(cstr);
		return *ppath == NULL;
	} else if (copy_part(cstr, p, ppath))
		return SMAP_URLE_NOMEM;
	else
		cstr = p + 1;
	*pport = strdup(cstr);
	return *pport == NULL ? SMAP_URLE_NOMEM : 0;
}

int
split_connection(const char *cstr,
		 char **pproto, char **pport, char **ppath)
{
	const char *p;

	p = strchr(cstr, ':');
	if (!p) {
		*pproto = NULL;
		if (cstr[0] == '/') {
			if ((*pproto = strdup("unix")) == NULL)
				return -1;
			if ((*ppath = strdup(cstr)) == NULL) {
				free(*pproto);
				return SMAP_URLE_NOMEM;
			}
			*pport = 0;
		} else
			return SMAP_URLE_NOABS;
	} else {
		char *proto;
		if (copy_part(cstr, p, &proto))
			return SMAP_URLE_NOMEM;
		cstr = p + 1;
		if (cstr[0] == '/' && cstr[1] == '/') {
			int rc = split_url(proto, cstr + 2, pport, ppath);
			if (rc)
				free(proto);
			else
				*pproto = proto;
			return rc;
		} else {
			free(proto);
			return SMAP_URLE_INVALID;
		}
		*pproto = proto;
	}
	return 0;
}

int
parse_url0(const char *cstr,
	   char *proto, char *port, char *path,
	   struct sockaddr **psa, socklen_t *plen)
{
	union {
		struct sockaddr sa;
		struct sockaddr_in s_in;
		struct sockaddr_un s_un;
	} addr;
	socklen_t socklen;
	struct sockaddr *sa;

	if (!proto
	    || strcmp(proto, "unix") == 0 || strcmp(proto, "local") == 0) {
		if (port) {
			return SMAP_URLE_PORT;
			return -1;
		}

		if (strlen(path) > sizeof addr.s_un.sun_path)
			return SMAP_URLE_NAME2LONG;

		addr.sa.sa_family = PF_UNIX;
		socklen = sizeof(addr.s_un);
		strcpy(addr.s_un.sun_path, path);
	} else if (strcmp(proto, "inet") == 0) {
		short pnum;
		long num;
		char *p;

		addr.sa.sa_family = PF_INET;
		socklen = sizeof(addr.s_in);

		if (!port)
			return SMAP_URLE_NOPORT;

		num = pnum = strtol(port, &p, 0);
		if (*p == 0) {
			if (num != pnum)
				return SMAP_URLE_BADPORT;
			pnum = htons(pnum);
		} else {
			struct servent *sp = getservbyname(port, "tcp");
			if (!sp)
				return SMAP_URLE_UNKPORT;
			pnum = sp->s_port;
		}

		if (!path)
			addr.s_in.sin_addr.s_addr = INADDR_ANY;
		else {
			struct hostent *hp = gethostbyname(path);
			if (!hp)
				return SMAP_URLE_UNKHOST;
			addr.sa.sa_family = hp->h_addrtype;
			switch (hp->h_addrtype) {
			case AF_INET:
				memmove(&addr.s_in.sin_addr, hp->h_addr, 4);
				addr.s_in.sin_port = pnum;
				break;

			default:
				return SMAP_URLE_BADFAMILY;
			}
		}
	} else
		return SMAP_URLE_BADPROTO;
	sa = malloc(socklen);
	if (!sa)
		return SMAP_URLE_NOMEM;
	memcpy(sa, &addr, socklen);
	*psa = sa;
	*plen = socklen;
	return 0;
}

int
smap_url_parse(const char *cstr, struct sockaddr **psa, socklen_t *plen)
{
	int rc;
	char *proto = NULL, *port = NULL, *path = NULL;

	rc = split_connection(cstr, &proto, &port, &path);
	if (rc)
		return rc;
	rc = parse_url0(cstr, proto, port, path, psa, plen);
	free(proto);
	free(port);
	free(path);
	return rc;
}
